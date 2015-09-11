/* echoserver.c -- libco TCP echo server demo */
/* Copyright (C) 2015 Alex Iadicicco */

#include <co.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

static void echo_thread(co_context_t *co, void *_peer) {
	co_logger_t *log = co_logger(co, NULL);
	co_file_t *peer = _peer;
	char byte;
	size_t rsize;

	for (;;) {
		if (co_read(co, peer, &byte, 1, &rsize) < 0)
			break;
		if (rsize == 0)
			break;
		if (co_write(co, peer, &byte, 1, NULL) < 0)
			break;
	}

	co_info(log, "echo thread exiting!");
	co_close(co, peer);
}

static void main_thread(co_context_t *co, void *unused) {
	co_logger_t *log = co_logger(co, NULL);
	co_file_t *listener;

	co_info(log, "creating echo server on port 4321");
	listener = co_bind_tcp6(co, "::", 4321, 5);

	if (listener == NULL) {
		co_error(log, "could not create listener; stopping");
		return;
	}

	co_info(log, "now accepting connections");
	for (;;) {
		char buf[512];
		unsigned short port;
		co_file_t *peer = co_accept(co, listener, buf, 512, &port);

		co_info(log, "connection on %s:%d", buf, port);
		co_spawn(co, echo_thread, peer);
	}
}

int main(int argc, char *argv[]) {
	co_context_t *co;

	co = co_init();
	co_log_level(co, NULL, CO_LOG_DEBUG);

	co_run(co, main_thread, NULL);
}
