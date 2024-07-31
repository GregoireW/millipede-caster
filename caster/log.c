#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "log.h"

int log_init(struct log *this, const char *filename, log_cb_t log_cb, void *arg) {
    if (strcmp(filename, "stdout")==0){
        this->logfile = stdout;
    } else if (strcmp(filename, "stderr")==0){
        this->logfile = stderr;
    } else {
        this->logfile = fopen(filename, "a+");
    }
	if (!this->logfile) {
		fprintf(stderr, "Can't open log file %s: %s\n", filename, strerror(errno));
		return -1;
	}
	this->log_cb = log_cb;
	this->state = arg;
	setlinebuf(this->logfile);
	P_RWLOCK_INIT(&this->lock, NULL);
	return this->logfile == NULL ? -1:0;
}

void log_reopen(struct log *this, const char *filename) {
	FILE *newfile =	fopen(filename, "a+");
	if (!newfile) {
		fprintf(stderr, "Can't reopen log file %s: %s\n", filename, strerror(errno));
		return;
	}
	setlinebuf(newfile);
	P_RWLOCK_WRLOCK(&this->lock);
	fclose(this->logfile);
	this->logfile = newfile;
	P_RWLOCK_UNLOCK(&this->lock);
}

void log_free(struct log *this) {
	P_RWLOCK_WRLOCK(&this->lock);
	fclose(this->logfile);
	P_RWLOCK_DESTROY(&this->lock);
}

/*
static void
_log(struct log *this, const char *fmt, va_list ap) {
	char date[36];
	logdate(date, sizeof date);
	fputs(date, this->logfile);
	vfprintf(this->logfile, fmt, ap);
}
*/

void
logfmt(struct log *this, const char *fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	this->log_cb(this->state, fmt, ap);
	va_end(ap);
}
