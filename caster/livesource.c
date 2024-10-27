#include <assert.h>
#include <string.h>

#include <event2/buffer.h>
#include <event2/bufferevent.h>

#include "conf.h"
#include "caster.h"
#include "jobs.h"
#include "livesource.h"
#include "ntrip_common.h"
#include "ntripsrv.h"
#include "packet.h"
#include "queue.h"

static void _livesource_del_subscriber_unlocked(struct ntrip_state *st);

struct livesource *livesource_new(char *mountpoint, enum livesource_state state) {
	struct livesource *this = (struct livesource *)malloc(sizeof(struct livesource));
	if (this == NULL)
		return NULL;
	this->mountpoint = mystrdup(mountpoint);
	if (this->mountpoint == NULL) {
		free(this);
		return NULL;
	}
	TAILQ_INIT(&this->subscribers);
	this->nsubs = 0;
	this->npackets = 0;
	this->state = state;

	P_RWLOCK_INIT(&this->lock, NULL);
	return this;
}

/*
 * Kill some or all subscribers of a livesource.
 *
 * Required locks: lock on the livesource.
 *
 * If kill_backlogged is 0:
 *	unsubscribe users subscribed for a virtual source
 *	unsubscribe & kill others
 * If kill_backlogged is not 0:
 *	unsubscribe & kill subscribers flagged as backlogged
 */
int livesource_kill_subscribers_unlocked(struct livesource *this, int kill_backlogged) {
	struct subscriber *np, *tnp;
	int killed = 0;
	TAILQ_FOREACH_SAFE(np, &this->subscribers, next, tnp) {
		/* Keep a pointer because it will be possibly destroyed by ntrip_deferred_free() */
		struct bufferevent *bev = np->ntrip_state->bev;

		bufferevent_lock(bev);

		if (kill_backlogged ? np->backlogged : !np->virtual) {
			ntrip_log(np->ntrip_state, LOG_NOTICE, "dropping due to %s\n", kill_backlogged?"backlog":"closed source");
			killed++;
		} else if (kill_backlogged == 0 && np->virtual) {
			/*
			 * Try to resubscribe virtual sources to a new source
			 */
			joblist_append_ntrip_locked(np->ntrip_state->caster->joblist, np->ntrip_state, &ntripsrv_redo_virtual_pos);
		}

		if (kill_backlogged == 0 || np->backlogged) {
			struct ntrip_state *st = np->ntrip_state;
			_livesource_del_subscriber_unlocked(st);
			ntrip_deferred_free(st, "livesource_kill_subscribers_unlocked");
		}
		bufferevent_unlock(bev);
	}
	return killed;
}

void livesource_free(struct livesource *this) {
	P_RWLOCK_WRLOCK(&this->lock);
	livesource_kill_subscribers_unlocked(this, 0);
	P_RWLOCK_UNLOCK(&this->lock);
	P_RWLOCK_DESTROY(&this->lock);
	strfree(this->mountpoint);
	free(this);
}

void livesource_set_state(struct livesource *this, enum livesource_state state) {
	P_RWLOCK_WRLOCK(&this->lock);
	this->state = state;
	P_RWLOCK_UNLOCK(&this->lock);
}

/*
 * Add a subscriber to a live source.
 *
 * Required lock: ntrip_state
 */
struct subscriber *livesource_add_subscriber(struct livesource *this, struct ntrip_state *st) {
	struct subscriber *sub = (struct subscriber *)malloc(sizeof(struct subscriber));
	if (sub != NULL) {
		sub->livesource = this;
		sub->ntrip_state = st;
		sub->backlogged = 0;
		sub->virtual = 0;

		P_RWLOCK_WRLOCK(&this->lock);
		TAILQ_INSERT_TAIL(&this->subscribers, sub, next);
		this->nsubs++;
		st->subscription = sub;
		P_RWLOCK_UNLOCK(&this->lock);

		ntrip_log(st, LOG_INFO, "subscription done to %s\n", this->mountpoint);
	}
	return sub;
}

/*
 * Remove a subscriber from a live source.
 */
static void _livesource_del_subscriber_unlocked(struct ntrip_state *st) {
	if (st->subscription) {
		struct subscriber *sub = st->subscription;
		TAILQ_REMOVE(&sub->livesource->subscribers, sub, next);
		sub->livesource->nsubs--;
		sub->ntrip_state->subscription = NULL;
		free(sub);
	}
}

void livesource_del_subscriber(struct ntrip_state *st) {
	/*
	 * Lock order is mandatory to avoid deadlocks with livesource_send_subscribers
	 */
	P_MUTEX_LOCK(&st->caster->livesources.delete_lock);
	struct subscriber *sub = st->subscription;
	if (sub) {
		struct livesource *livesource = sub->livesource;
		P_RWLOCK_WRLOCK(&livesource->lock);
		bufferevent_lock(st->bev);

		_livesource_del_subscriber_unlocked(st);

		bufferevent_unlock(st->bev);
		P_RWLOCK_UNLOCK(&livesource->lock);
	}
	P_MUTEX_UNLOCK(&st->caster->livesources.delete_lock);
}

/*
 * Packet freeing callback
 */
static void raw_free_callback(const void *data, size_t datalen, void *extra) {
	struct packet *packet = (struct packet *)extra;
	packet_free(packet);
}

/*
 * Send a packet to all source subscribers
 *
 * Required locks: ntrip_state, packet
 */
int livesource_send_subscribers(struct livesource *this, struct packet *packet, struct caster_state *caster) {
	struct subscriber *np;
	int n = 0;
	int ns = 0;

	if (this == NULL)
		/* Dead livesource */
		return 0;

	P_RWLOCK_WRLOCK(&this->lock);

	this->npackets++;

	/* Increase reference count in one go to reduce overhead */
	if (packet->caster->config->zero_copy) {
		P_MUTEX_LOCK(&packet->mutex);
		packet->refcnt += this->nsubs;
		P_MUTEX_UNLOCK(&packet->mutex);
	}

	int nbacklogged = 0;

	TAILQ_FOREACH(np, &this->subscribers, next) {
		struct ntrip_state *st = np->ntrip_state;
		struct bufferevent *bev = st->bev;
		bufferevent_lock(bev);
		if (st->state == NTRIP_END) {
			/* Subscriber currently closing, skip */
			ntrip_log(st, LOG_DEBUG, "livesource_send_subscribers: dropping, state=%d\n", st->state);
			bufferevent_unlock(bev);
			ns++;
			n++;
			continue;
		}
		size_t backlog_len = evbuffer_get_length(bufferevent_get_output(st->bev));
		if (backlog_len > caster->config->backlog_evbuffer) {
			ntrip_log(st, LOG_NOTICE, "RTCM: backlog len %ld on output for %s\n", backlog_len, this->mountpoint);
			np->backlogged = 1;
			nbacklogged++;
		} else if (packet->caster->config->zero_copy) {
			if (evbuffer_add_reference(bufferevent_get_output(st->bev), packet->data, packet->datalen, raw_free_callback, packet) < 0) {
				ntrip_log(st, LOG_CRIT, "RTCM: evbuffer_add_reference failed\n");
				ns++;
			} else
				st->sent_bytes += packet->datalen;
		} else {
			if (evbuffer_add(bufferevent_get_output(st->bev), packet->data, packet->datalen) < 0) {
				ns++;
			} else
				st->sent_bytes += packet->datalen;
		}
		bufferevent_unlock(bev);
		n++;
	}

	assert(n == this->nsubs);

	/*
	 * Adjust reference count to account for failed calls
	 */
	if (packet->caster->config->zero_copy && ns) {
		/* Don't need to free the packet as it will be done by the caller, the refcnt should never be 0 here */
		P_MUTEX_LOCK(&packet->mutex);
		packet->refcnt -= ns;
		P_MUTEX_UNLOCK(&packet->mutex);
	}
	assert(packet->refcnt > 0);

	/*
	 * Get rid of backlogged connections
	 */
	if (nbacklogged) {
		int found_backlogs = livesource_kill_subscribers_unlocked(this, 1);
		if (found_backlogs == nbacklogged)
			logfmt(&caster->flog, "RTCM: %d backlogged clients dropped from %s\n", nbacklogged, this->mountpoint);
		else
			logfmt(&caster->flog, "RTCM: %d (expected %d) backlogged clients dropped from %s\n", found_backlogs, nbacklogged, this->mountpoint);
	}
	P_RWLOCK_UNLOCK(&this->lock);

	if (n && (this->npackets == 1 || (this->npackets % 100 == 0)))
		logfmt(&caster->flog, "RTCM: %d packets sent, current one to %d subscribers for %s\n", this->npackets, n, this->mountpoint);
	return n;
}

/*
 * Display a list of current live sources.
 */
static void livesource_list(struct caster_state *caster) {
	struct livesource *np;

	P_RWLOCK_RDLOCK(&caster->livesources.lock);

	TAILQ_FOREACH(np, &caster->livesources.queue, next) {
		logfmt(&caster->flog, "Live:");
		break;
	}
	TAILQ_FOREACH(np, &caster->livesources.queue, next) {
		logfmt(&caster->flog, " %s", np->mountpoint);
	}
	TAILQ_FOREACH(np, &caster->livesources.queue, next) {
		logfmt(&caster->flog, "\n");
		break;
	}

	P_RWLOCK_UNLOCK(&caster->livesources.lock);
}

/*
 * Find a livesource by mountpoint name.
 * Warning: O(n) complexity.
 *
 * Required lock (read): livesource list.
 */
struct livesource *livesource_find_unlocked(struct caster_state *this, struct ntrip_state *st, char *mountpoint, pos_t *mountpoint_pos, int on_demand, enum livesource_state *new_state) {
	struct livesource *np;
	struct livesource *result = NULL;
	TAILQ_FOREACH(np, &this->livesources.queue, next) {
		if (!strcmp(np->mountpoint, mountpoint)
			&& (np->state == LIVESOURCE_RUNNING
			    || (on_demand && np->state == LIVESOURCE_FETCH_PENDING))) {
			result = np;
			break;
		}
	}

	if (result == NULL && on_demand && st) {
		struct livesource *np = livesource_new(mountpoint, LIVESOURCE_FETCH_PENDING);
		if (np == NULL) {
			return NULL;
		}
		TAILQ_INSERT_TAIL(&this->livesources.queue, np, next);
		ntrip_log(st, LOG_INFO, "Trying to subscribe to on-demand source %s\n", mountpoint);
		struct redistribute_cb_args *redis_args = redistribute_args_new(this, np, mountpoint, mountpoint_pos, this->config->reconnect_delay, 0);
		joblist_append_redistribute(this->joblist, redistribute_source_stream, redis_args);
		result = np;
	}

	/* Copy current state while we are locked */
	if (new_state && result)
		*new_state = result->state;
	return result;
}

/*
 * Find a livesource by mountpoint name.
 * Warning: O(n) complexity.
 */
struct livesource *livesource_find_on_demand(struct caster_state *this, struct ntrip_state *st, char *mountpoint, pos_t *mountpoint_pos, int on_demand, enum livesource_state *new_state) {
	P_RWLOCK_RDLOCK(&this->livesources.lock);
	struct livesource *result = livesource_find_unlocked(this, st, mountpoint, mountpoint_pos, on_demand, new_state);
	P_RWLOCK_UNLOCK(&this->livesources.lock);
	return result;
}

struct livesource *livesource_find(struct caster_state *this, struct ntrip_state *st, char *mountpoint, pos_t *mountpoint_pos) {
	return livesource_find_on_demand(this, st, mountpoint, mountpoint_pos, 0, NULL);
}
