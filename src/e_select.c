/* e_select.c -- select-based event backend */
/* Copyright (C) 2015 Alex Iadicicco */

#include "event.h"

#include <stdlib.h>
#include <sys/select.h>
#include <sys/time.h>

/* maybe these can be replaced with big ring buffers for FASTFASTFAST? */
struct waiting {
	int fd;
	struct waiting *next, *prev;
};

static int fd_max;
static fd_set fds_waiting_read;
static fd_set fds_waiting_write;

static struct waiting waiting_queue;

void event_init(void) {
	fd_max = 0;
	FD_ZERO(&fds_waiting_read);
	FD_ZERO(&fds_waiting_write);
	waiting_queue.next = &waiting_queue;
	waiting_queue.prev = &waiting_queue;
}

static void add_to_queue(int fd) {
	struct waiting *w = calloc(1, sizeof(*w));
	w->fd = fd;
	w->next = &waiting_queue;
	w->prev = waiting_queue.prev;
	w->next->prev = w;
	w->prev->next = w;
}

void event_fd_want_read(int fd) {
	if (fd > fd_max) fd_max = fd;
	FD_SET(fd, &fds_waiting_read);
	add_to_queue(fd);
}

void event_fd_want_write(int fd) {
	if (fd > fd_max) fd_max = fd;
	FD_SET(fd, &fds_waiting_write);
	add_to_queue(fd);
}

bool event_poll(event_polled_t *result, struct timeval *timeout) {
	fd_set r, w, x;
	struct waiting *cur;

	r = fds_waiting_read;
	w = fds_waiting_write;
	FD_ZERO(&x);

	result->tag = EVENT_NOTHING;

	select(fd_max+1, &r, &w, &x, timeout);

	for (
		cur = waiting_queue.next;
		cur != &waiting_queue;
		cur = cur->next
	) {
		if (FD_ISSET(cur->fd, &r)) {
			result->tag = EVENT_FD_CAN_READ;
			result->v.fd = cur->fd;
			FD_CLR(cur->fd, &fds_waiting_read);
			cur->next->prev = cur->prev;
			cur->prev->next = cur->next;
			free(cur);
			return true;
		}
		if (FD_ISSET(cur->fd, &w)) {
			result->tag = EVENT_FD_CAN_WRITE;
			result->v.fd = cur->fd;
			FD_CLR(cur->fd, &fds_waiting_write);
			cur->next->prev = cur->prev;
			cur->prev->next = cur->next;
			free(cur);
			return true;
		}
	}

	return false;
}
