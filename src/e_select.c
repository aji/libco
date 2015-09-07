/* e_select.c -- select-based event backend */
/* Copyright (C) 2015 Alex Iadicicco */

#include "event.h"

#include <sys/select.h>
#include <sys/time.h>

static int fd_max;
static fd_set fds_waiting_read;
static fd_set fds_waiting_write;
static fd_set fds_waiting;

void event_init(void) {
	fd_max = 0;
	FD_ZERO(&fds_waiting_read);
	FD_ZERO(&fds_waiting_write);
}

void event_fd_want_read(int fd) {
	if (fd > fd_max) fd_max = fd;
	FD_SET(fd, &fds_waiting_read);
	FD_SET(fd, &fds_waiting);
}

void event_fd_want_write(int fd) {
	if (fd > fd_max) fd_max = fd;
	FD_SET(fd, &fds_waiting_write);
	FD_SET(fd, &fds_waiting);
}

bool event_poll(event_polled_t *result) {
	fd_set r, w, x;
	struct timeval tv;
	int fd;

	r = fds_waiting_read;
	w = fds_waiting_write;
	x = fds_waiting;

	tv.tv_sec = 1;
	tv.tv_usec = 0;

	result->tag = EVENT_NOTHING;

	select(fd_max+1, &r, &w, &x, &tv);

	/* these loops are pretty nasty. optimization opportunity! */

	for (fd=0; fd<=fd_max; fd++) {
		if (FD_ISSET(fd, &r)) {
			result->tag = EVENT_FD_CAN_READ;
			result->v.fd = fd;
			FD_CLR(fd, &fds_waiting_read);
			if (!FD_ISSET(fd, &fds_waiting_write))
				FD_CLR(fd, &fds_waiting);
			return true;
		}
	}

	for (fd=0; fd<=fd_max; fd++) {
		if (FD_ISSET(fd, &w)) {
			result->tag = EVENT_FD_CAN_WRITE;
			result->v.fd = fd;
			FD_CLR(fd, &fds_waiting_write);
			if (!FD_ISSET(fd, &fds_waiting_read))
				FD_CLR(fd, &fds_waiting);
			return true;
		}
	}

	for (fd=0; fd<=fd_max; fd++) {
		if (FD_ISSET(fd, &x)) {
			result->tag = EVENT_FD_ERROR;
			result->v.fd = fd;
			FD_CLR(fd, &fds_waiting_read);
			FD_CLR(fd, &fds_waiting_write);
			FD_CLR(fd, &fds_waiting);
			return true;
		}
	}

	return false;
}
