#ifndef __JOBS_H__
#define __JOBS_H__

#include <event2/bufferevent.h>
#include "queue.h"

/*
 * Job entry for the FIFO list, to dispatch tasks to workers.
 * Only used when threads are activated.
 */
struct job {
	STAILQ_ENTRY(job) next;

	/* If not NULL, this is a read or write callback job */
	void (*cb)(struct bufferevent *bev, void *arg);

	/* If not NULL, this is an event callback job */
	void (*cbe)(struct bufferevent *bev, short events, void *arg);

	/* Parameter for all jobs */
	void *arg;

	/* Event flags for event jobs only */
	short events;
};
STAILQ_HEAD (jobq, job);
STAILQ_HEAD (ntripq, ntrip_state);
TAILQ_HEAD (general_ntripq, ntrip_state);

/*
 *  FIFO list for worker threads to get new jobs.
 */
struct joblist {
	/* The queue itself */
	struct ntripq ntrip_queue;

	/* Protect access to the queue */
	P_MUTEX_T mutex;

	/* Used to signal workers a new job has been appended */
	pthread_cond_t condjob;

	/* The associated caster */
	struct caster_state *caster;
};

struct joblist *joblist_new(struct caster_state *caster);
void joblist_free(struct joblist *this);
void joblist_run(struct joblist *this);
void joblist_append(struct joblist *this, void (*cb)(struct bufferevent *bev, void *arg), void (*cbe)(struct bufferevent *bev, short events, void *arg), struct bufferevent *bev, void *arg, short events);
void *jobs_start_routine(void *arg);
int jobs_start_threads(struct caster_state *caster, int nthreads);

#ifndef STAILQ_LAST
// STAILQ_LAST is not defined in linux queue.h. It is only on BSD systems.
// This is a copy of the definition from FreeBSD's queue.h
// here is the associated license:
/*
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)queue.h	8.5 (Berkeley) 8/20/94
 */
#define	STAILQ_LAST(head, type, field)					\
	(STAILQ_EMPTY((head)) ?						\
		NULL :							\
	        ((struct type *)(void *)				\
		((char *)((head)->stqh_last) - offsetof(struct type, field))))

#endif

#endif
