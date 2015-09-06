/* thread.h -- threading prototypes */
/* Copyright (C) 2015 Alex Iadicicco */

#ifndef __INC_CO_THREAD_H__
#define __INC_CO_THREAD_H__

typedef struct thread_context thread_context_t;
typedef struct thread thread_t;
typedef void thread_fn(thread_context_t*, void*);
typedef thread_t *thread_poll_fn(thread_context_t*, void*);

extern thread_context_t *thread_context_new(void);
extern void thread_context_run(thread_context_t*, thread_poll_fn*, void*);
extern void thread_context_stop(thread_context_t*);

/* the newly created thread will not be run unless it is returned from the
   poll function */
extern thread_t *thread_create(thread_context_t*, thread_fn*, void*);

extern thread_t *thread_self(thread_context_t*);
extern void thread_defer_self(thread_context_t*);

#endif
