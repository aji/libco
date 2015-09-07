/* co.c -- libco */
/* Copyright (C) 2015 Alex Iadicicco */

#include <co.h>

#include "event.h"
#include "thread.h"

#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>

#define error(x) abort()

struct co_context {
	thread_context_t *threads;
	int num_threads;
	struct new_thread *new_thread;
	co_file_t *files;
};

struct co_file {
	thread_t *waiting;
	int fd;
	co_file_t *next;
};

struct new_thread {
	struct new_thread *next;
	co_context_t *ctx;
	co_thread_fn *start;
	void *user;
	thread_t *t;
};

static void start_thread(thread_context_t *ctx, void *priv) {
	struct new_thread *nt = priv;
	nt->start(nt->ctx, nt->user);
	nt->ctx->num_threads --;
}

co_file_t *co_open(
	co_context_t                  *ctx,
	const char                    *path,
	co_open_type_t                 typ,
	unsigned                       mode
) {
	int fd;
	co_file_t *f;

	if (!(fd = open(path, O_RDONLY)))
		return NULL;

	f = calloc(1, sizeof(*f));
	f->waiting = NULL;
	f->fd = fd;
	f->next = ctx->files;
	ctx->files = f;

	return f;
}

co_err_t co_spawn(
	co_context_t                  *ctx,
	co_thread_fn                  *start,
	void                          *user
) {
	struct new_thread *next;

	next = calloc(1, sizeof(*next));
	next->ctx = ctx;
	next->start = start;
	next->user = user;
	next->t = thread_create(ctx->threads, start_thread, next);
	ctx->new_thread = next;
	ctx->num_threads ++;

	return 0;
}

co_err_t co_read(
	co_context_t                  *ctx,
	co_file_t                     *file,
	void                          *buf,
	size_t                         nbyte,
	ssize_t                       *rsize
) {
	ssize_t rsz;

	if (file->waiting != NULL) {
		error("another context is already waiting on this file");
		return -1;
	}

	file->waiting = thread_self(ctx->threads);
	event_fd_want_read(file->fd);

	thread_defer_self(ctx->threads);

	rsz = read(file->fd, buf, nbyte);
	if (rsize) *rsize = rsz;
	return 0;
}

co_err_t co_write(
	co_context_t                  *ctx,
	co_file_t                     *file,
	const void                    *buf,
	size_t                         nbyte,
	ssize_t                       *wsize
) {
	ssize_t wsz;

	if (file->waiting != NULL) {
		error("another context is already waiting on this file");
		return -1;
	}

	file->waiting = thread_self(ctx->threads);
	event_fd_want_write(file->fd);

	thread_defer_self(ctx->threads);

	wsz = write(file->fd, buf, nbyte);
	if (wsize) *wsize = wsz;
	return 0;
}

co_context_t *co_init(void) {
	co_context_t *ctx;

	ctx = calloc(1, sizeof(*ctx));
	ctx->threads = thread_context_new();
	ctx->new_thread = NULL;
	ctx->files = NULL;

	event_init();

	return ctx;
}

static thread_t *unwait(co_context_t *ctx, int fd) {
	for (f=ctx->files; f; f=f->next) {
		if (f->fd == fd) {
			thread_t *t = f->waiting;
			f->waiting = NULL;
			return t;
		}
	}

	return NULL;
}

static thread_t *thread_poller(thread_context_t *threads, void *_ctx) {
	thread_t *res;
	co_context_t *ctx = _ctx;
	event_polled_t evt;
	co_file_t *f;

	if (ctx->new_thread) {
		struct new_thread *next = ctx->new_thread->next;
		res = ctx->new_thread->t;
		free(ctx->new_thread);
		ctx->new_thread = next;
		return res;
	}

	if (event_poll(&evt)) {
		switch (evt.tag) {
		case EVENT_NOTHING:
			break;
		case EVENT_FD_CAN_READ:
		case EVENT_FD_CAN_WRITE:
			return unwait(ctx, evt.v.fd);
		}
	}

	if (ctx->num_threads == 0)
		thread_context_stop(threads);

	return NULL;
}

void co_run(
	co_context_t                  *ctx,
	co_thread_fn                  *start,
	void                          *user
) {
	co_spawn(ctx, start, user);

	thread_context_run(ctx->threads, thread_poller, ctx);
}
