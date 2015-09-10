/* event.h -- event handling prototypes */
/* Copyright (C) 2015 Alex Iadicicco */

#ifndef __INC_CO_EVENT_H__
#define __INC_CO_EVENT_H__

#include <stdbool.h>
#include <sys/time.h>

typedef struct event_polled event_polled_t;

struct event_polled {
	enum {
		EVENT_NOTHING,              /* (none) */
		EVENT_FD_CAN_READ,          /* fd */
		EVENT_FD_CAN_WRITE,         /* fd */
		EVENT_FD_ERROR,             /* fd */
	} tag;

	union {
		int fd;
	} v;
};

extern void event_init(void);

extern void event_fd_want_read(int fd);
extern void event_fd_want_write(int fd);

extern bool event_poll(event_polled_t *result, struct timeval *timeout);

#endif
