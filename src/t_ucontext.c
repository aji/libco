/* t_ucontext.c -- ucontext threading implementation */
/* Copyright (C) 2015 Alex Iadicicco */

#include "thread.h"

#include <stdbool.h>
#include <stdlib.h>
#include <ucontext.h>

#define STACK_SIZE (1<<20)

#if __SIZEOF_POINTER__ > __SIZEOF_INT__
# if __SIZEOF_INT__ < 4
#  error your integers are too small
# elif __SIZEOF_POINTER__ > 8
#  error your pointers are too big
# else
#  define _SPLIT_PTR
# endif
#endif

/* I have only tested this on 32-bit and 64-bit x86 setups! */

#ifdef _SPLIT_PTR
#define DEPTR(p0,p1) ((void*)( \
	(((unsigned long)(p0)) << 32) | \
	(((unsigned long)(p1)) <<  0)))
#define PTR(x) \
	(((unsigned long)(x)   >> 32) & 0xffffffff), \
	(((unsigned long)(x)   >>  0) & 0xffffffff)
#else
#define DEPTR(p0,p1) ((void*)((unsigned)(p0)))
#define PTR(x) ((unsigned)(x)), 0
#endif

struct thread_context {
	ucontext_t poller;
	thread_t *self;
	bool running;
};

struct thread {
	ucontext_t context;
	thread_fn *start;
	void *user;
};

static void start_thread(int c0, int c1, int t0, int t1) {
	thread_context_t *ctx = DEPTR(c0,c1);
	thread_t *t = DEPTR(t0,t1);
	t->start(ctx, t->user);
}

thread_context_t *thread_context_new(void) {
	return calloc(1, sizeof(thread_context_t));
}

void thread_context_run(thread_context_t *ctx, thread_poll_fn *fn, void *user) {
	thread_t *next;

	ctx->running = true;

	for (;;) {
		for (next = NULL; !next; ) {
			if (!ctx->running) {
				return;
			}

			next = fn(ctx, user);
		}

		ctx->self = next;
		swapcontext(&ctx->poller, &next->context);
	}
}

void thread_context_stop(thread_context_t *ctx) {
	ctx->running = false;
}

thread_t *thread_create(thread_context_t *ctx, thread_fn *start, void *user) {
	thread_t *t;

	t = calloc(1, sizeof(*t));
	t->start = start;
	t->user = user;
	getcontext(&t->context);
	t->context.uc_stack.ss_sp = malloc(STACK_SIZE);
	t->context.uc_stack.ss_size = STACK_SIZE;
	t->context.uc_link = &ctx->poller;
	makecontext(&t->context, start_thread, 4, PTR(ctx), PTR(t));

	return t;
}

thread_t *thread_self(thread_context_t *ctx) {
	return ctx->self;
}

void thread_defer_self(thread_context_t *ctx) {
	swapcontext(&ctx->self->context, &ctx->poller);
}
