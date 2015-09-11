/* co.c -- libco */
/* Copyright (C) 2015 Alex Iadicicco */

#include <co.h>

#include "event.h"
#include "thread.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#define READBUF_SIZE 2048

#define error(x) abort()

typedef struct co_timer co_timer_t;

struct co_file {
	thread_t *waiting;
	int fd;
	char readbuf[READBUF_SIZE];
	int readbuflen;
	co_file_t *next;
	co_file_t *prev;
};

struct co_logger {
	co_logger_t *inherit;
	co_log_level_t log_level;
	int refcount;
};

struct co_timer {
	thread_t *waiting;
	struct timeval fire;
	co_timer_t *next;
};

struct co_context {
	thread_context_t *threads;
	int num_threads;
	struct new_thread *new_thread;
	co_file_t *files;
	co_logger_t log;
	co_timer_t *timers;
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
	free(nt);
}

static co_file_t *new_file(int fd) {
	co_file_t *f = calloc(1, sizeof(*f));
	f->fd = fd;
	return f;
}

static void add_file(co_context_t *ctx, co_file_t *f) {
	if (ctx->files) ctx->files->prev = f;

	f->next = ctx->files;
	f->prev = NULL;

	ctx->files = f;
}

static void remove_file(co_context_t *ctx, co_file_t *f) {
	if (ctx->files == f) ctx->files = f->next;

	if (f->next) f->next->prev = f->prev;
	if (f->prev) f->prev->next = f->next;
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
	next->next = ctx->new_thread;
	ctx->new_thread = next;
	ctx->num_threads ++;

	co_debug(&ctx->log, "spawning a new thread: start=%p user=%p",
		start, user);

	return 0;
}

co_err_t co_read(
	co_context_t                  *ctx,
	co_file_t                     *file,
	void                          *_buf,
	size_t                         nbyte,
	ssize_t                       *rsize
) {
	unsigned char *buf = _buf;
	ssize_t rsz, total;

	if (file->waiting != NULL) {
		error("another context is already waiting on this file");
		return -1;
	}

	if (nbyte > 1) {
		co_trace(&ctx->log, "begin read: file=%p buf=%p nbyte=%d",
			file, buf, nbyte);
	}

	total = 0;

	/* first fulfill as much of the request with the read buffer as we can */
	if (file->readbuflen > 0) {
		rsz = file->readbuflen;
		if (rsz > nbyte) rsz = nbyte;
		memcpy(buf, file->readbuf, rsz);
		buf += rsz;
		total += rsz;
		nbyte -= rsz;
		file->readbuflen -= rsz;
		memmove(file->readbuf, file->readbuf + rsz, file->readbuflen);
	}

	/* either the read buffer satisfied the entire request, or the user
	 * actually asked for 0 bytes... */
	if (nbyte == 0) {
		if (rsize) *rsize = total;
		return 0;
	}

	/* if the read buffer couldn't satisfy the whole request, then do an
	 * actual read. reads for more bytes than the read buffer can hold pass
	 * directly through to the user. otherwise, we attempt to read into the
	 * entire read buffer and do one last copy out of it for the call. */
	file->waiting = thread_self(ctx->threads);
	event_fd_want_read(file->fd);
	thread_defer_self(ctx->threads);

	if (nbyte > READBUF_SIZE) {
		rsz = read(file->fd, buf, nbyte);
		if (rsz < 0) { // TODO: what do we do..
			if (rsize) *rsize = total;
			return total > 0 ? 0 : -1;
		} else {
			total += rsz;
		}
	} else {
		rsz = read(
			file->fd,
			file->readbuf,
			READBUF_SIZE - file->readbuflen
		);
		if (rsz < 0) { // TODO: what do we do..
			if (rsize) *rsize = total;
			return total > 0 ? 0 : -1;
		}
		file->readbuflen = rsz;
		if (rsz > nbyte) rsz = nbyte;
		memcpy(buf, file->readbuf, rsz);
		buf += rsz;
		total += rsz;
		nbyte -= rsz;
		file->readbuflen -= rsz;
		memmove(file->readbuf, file->readbuf + rsz, file->readbuflen);
	}

	if (rsize) *rsize = total;
	return 0;
}

bool co_read_line(
	co_context_t                  *ctx,
	co_file_t                     *file,
	void                          *_buf,
	size_t                         nbyte
) {
	int total;
	unsigned char byte, *buf = _buf;
	ssize_t rsz;

	total = 0;

	co_trace(&ctx->log, "begin read line: file=%p buf=%p nbyte=%d",
		file, buf, nbyte);

	while (nbyte > 1) {
		co_read(ctx, file, &byte, 1, &rsz);
		if (rsz == 0)
			break;
		total++;
		if (byte == '\r' && nbyte > 2) {
			co_read(ctx, file, &byte, 1, &rsz);
			total++;
			if (rsz == 0) {
				*buf++ = '\r';
				nbyte--;
				break;
			} else if (byte != '\n') {
				*buf++ = '\r';
				*buf++ = byte;
				nbyte -= 2;
				break;
			}

			if (rsz == 0 || byte != '\n') {
				*buf++ = '\r';
				*buf++ = byte;
				nbyte -= 2;
				if (rsz == 0)
					break;
			} else if (byte == '\n') {
				break;
			}
		}
		if (byte == '\n')
			break;
		*buf++ = byte;
		nbyte--;
	}

	*buf = '\0';
	return total > 0;
}

co_err_t co_write(
	co_context_t                  *ctx,
	co_file_t                     *file,
	const void                    *buf,
	size_t                         nbyte,
	ssize_t                       *wsize
) {
	ssize_t wsz;

	co_trace(&ctx->log, "begin write: file=%p buf=%p nbyte=%d",
		file, buf, nbyte);

	if (file->waiting != NULL) {
		error("another context is already waiting on this file");
		return -1;
	}

	file->waiting = thread_self(ctx->threads);
	event_fd_want_write(file->fd);

	thread_defer_self(ctx->threads);

	wsz = write(file->fd, buf, nbyte);
	if (wsz < 0) {
		if (wsize) *wsize = 0;
		return -1;
	} else {
		if (wsize) *wsize = wsz;
		return 0;
	}
}

co_file_t *co_open(
	co_context_t                  *ctx,
	const char                    *path,
	co_open_type_t                 typ,
	unsigned                       mode
) {
	int fd;
	co_file_t *f;

	co_trace(&ctx->log, "opening file: %s", path);

	if (!(fd = open(path, O_RDONLY)))
		return NULL;

	f = new_file(fd);
	add_file(ctx, f);

	return f;
}

void co_close(
	co_context_t                  *ctx,
	co_file_t                     *f
) {
	if (f == NULL)
		return;

	co_trace(&ctx->log, "closing file");

	remove_file(ctx, f);

	close(f->fd);
	free(f);
}

co_file_t *co_connect_tcp(
	co_context_t                  *ctx,
	const char                    *host,
	unsigned short                 port
) {
	struct addrinfo gai_hints;
	struct addrinfo *gai;
	struct sockaddr_in *sa4;
	struct sockaddr_in6 *sa6;
	void *addr;
	int sock, err;
	co_file_t *f;
	char buf[512];

	co_info(&ctx->log, "connect to %s:%d", host, port);

	/* TODO: replace this with an async lookup mechanism */
	memset(&gai_hints, 0, sizeof(gai_hints));
	gai_hints.ai_family = AF_INET;
	gai_hints.ai_socktype = SOCK_STREAM;
	gai_hints.ai_protocol = 0;
	if ((err = getaddrinfo(host, NULL, &gai_hints, &gai)) != 0) {
		co_error(
			&ctx->log,
			"co_connect_tcp failed: %s",
			gai_strerror(err)
		);
		return NULL;
	}
	if (gai->ai_addr == NULL)
		goto fail_addr;

	switch (gai->ai_family) {
	case AF_INET:
		sa4 = (void*)gai->ai_addr;
		sa4->sin_port = htons(port);
		addr = &sa4->sin_addr;
		break;
	case AF_INET6:
		sa6 = (void*)gai->ai_addr;
		sa6->sin6_port = htons(port);
		addr = &sa6->sin6_addr;
		break;
	default:
		goto fail_addr;
	}

	if (inet_ntop(gai->ai_family, addr, buf, 512)) {
		co_debug(&ctx->log, "address resolves to %s", buf);
	} else {
		snprintf(buf, 512, "<inet_ntop failure>");
		co_notice(&ctx->log, "inet_ntop failed?");
	}

	if ((sock = socket(gai->ai_family, SOCK_STREAM, 0)) < 0)
		goto fail_addr;
	fcntl(sock, F_SETFL, O_NONBLOCK);

	f = new_file(sock);
	add_file(ctx, f);

	co_debug(&ctx->log, "starting connection attempt...");
	for (;;) {
		err = connect(sock, gai->ai_addr, gai->ai_addrlen);
		if (err == 0)
			break;

		switch (errno) {
		case EINPROGRESS:
		case EINTR:
		case EALREADY:
			co_trace(&ctx->log, "connection not ready; deferring");
			f->waiting = thread_self(ctx->threads);
			event_fd_want_write(f->fd);
			thread_defer_self(ctx->threads);
			co_debug(&ctx->log, "attempting to complete connection");
			continue;

		default:
			goto fail_connect;
		}
	}

	co_debug(&ctx->log, "connection to %s:%d ready", host, port);
	freeaddrinfo(gai);
	return f;

fail_connect:
	co_error(&ctx->log, "connection failed: %s", strerror(errno));
	remove_file(ctx, f);
	free(f);
	close(sock);
fail_addr:
	freeaddrinfo(gai);
	return NULL;
}

co_file_t *co_bind_tcp6(
	co_context_t                  *ctx,
	const char                    *host,
	unsigned short                 port,
	int                            backlog
) {
	int sock, one;
	struct sockaddr_in6 sa6;
	co_file_t *f;

	memset(&sa6, 0, sizeof(sa6));
	if (inet_pton(AF_INET6, host, &sa6.sin6_addr) < 0)
		return NULL;
	sa6.sin6_family = AF_INET6;
	sa6.sin6_port = htons(port);

	if ((sock = socket(AF_INET6, SOCK_STREAM, 0)) < 0)
		return NULL;
	fcntl(sock, F_SETFL, O_NONBLOCK);

	one = 1;
	if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one)) < 0)
		goto fail_bind;

	if (bind(sock, (void*)&sa6, sizeof(sa6)) < 0)
		goto fail_bind;

	if (listen(sock, backlog) < 0)
		goto fail_bind;

	f = new_file(sock);
	add_file(ctx, f);

	return f;

fail_bind:
	co_error(&ctx->log, "bind failed: %s", strerror(errno));
	close(sock);
	return NULL;
}

co_file_t *co_accept(
	co_context_t                  *ctx,
	co_file_t                     *file,
	char                          *addrbuf,
	size_t                         addrbufsize,
	unsigned short                *port
) {
	co_file_t *peer;
	struct sockaddr_storage ss;
	struct sockaddr_in *sa4;
	struct sockaddr_in6 *sa6;
	socklen_t ssize;
	int sock;
	int af = -1;
	void *addr;

	co_debug(&ctx->log, "waiting for a connection...");
	for (;;) {
		ssize = sizeof(ss);
		sock = accept(file->fd, (void*)&ss, &ssize);
		if (sock >= 0)
			break;

		switch (errno) {
		case EAGAIN:
			co_trace(&ctx->log, "no connection ready; deferring");
			file->waiting = thread_self(ctx->threads);
			event_fd_want_read(file->fd);
			thread_defer_self(ctx->threads);
			continue;
		case EINTR:
			co_trace(&ctx->log, "interrupted by signal; trying again");
			continue;
		default:
			co_error(&ctx->log, "accept failed: %s", strerror(errno));
			return NULL;
		}
	}

	fcntl(sock, F_SETFL, O_NONBLOCK);

	peer = new_file(sock);
	add_file(ctx, peer);

	if (port) *port = 0;

	switch (ssize) {
	case sizeof(struct sockaddr_in):
		af = AF_INET;
		sa4 = (void*)&ss;
		addr = &sa4->sin_addr;
		if (port) *port = ntohs(sa4->sin_port);
		break;
	case sizeof(struct sockaddr_in6):
		af = AF_INET6;
		sa6 = (void*)&ss;
		addr = &sa6->sin6_addr;
		if (port) *port = ntohs(sa6->sin6_port);
		break;
	}

	if (addrbuf != NULL) {
		if (af < 0 || !inet_ntop(af, addr, addrbuf, addrbufsize)) {
			snprintf(addrbuf, addrbufsize, "<unknown>");
		}
	}

	return peer;
}

static void make_timer(
	co_context_t *ctx,
	thread_t *waiting,
	struct timeval *fire
) {
	co_timer_t *timer, *cur;

	timer = calloc(1, sizeof(*timer));
	timer->waiting = waiting;
	timer->fire.tv_sec  = fire->tv_sec;
	timer->fire.tv_usec = fire->tv_usec;

	/* sorted insert into ctx->timers, sorted by fire time */

	/* if list empty, easy, just put it as the whole list */
	if (ctx->timers == NULL) {
		ctx->timers = timer;
		return;
	}

	/* if before first element, easy, just put as start of list */
	if (timercmp(&timer->fire, &ctx->timers->fire, <)) {
		timer->next = ctx->timers;
		ctx->timers = timer;
		return;
	}

	/* otherwise, insert after last timer before it */
	for (cur = ctx->timers; cur->next; cur = cur->next) {
		if (timercmp(&timer->fire, &cur->next->fire, <)) {
			timer->next = cur->next;
			cur->next = timer;
			return;
		}
	}

	/* otherwise, goes at the very end! cur is last timer with a ->next */
	cur->next = timer;
}

void co_usleep(
	co_context_t                  *ctx,
	unsigned long                  usecs
) {
	struct timeval now;

	gettimeofday(&now, NULL);

	now.tv_sec  += usecs / 1000000;
	now.tv_usec += usecs % 1000000;

	now.tv_sec  += now.tv_usec / 1000000;
	now.tv_usec  = now.tv_usec % 1000000;

	make_timer(ctx, thread_self(ctx->threads), &now);
	thread_defer_self(ctx->threads);
}

void co_sleep(
	co_context_t                  *ctx,
	unsigned long                  secs
) {
	struct timeval now;

	gettimeofday(&now, NULL);
	now.tv_sec += secs;

	make_timer(ctx, thread_self(ctx->threads), &now);
	thread_defer_self(ctx->threads);
}

static const char *log_level_names[] = {
	[CO_LOG_TRACE]    = "TRACE",
	[CO_LOG_DEBUG]    = "DEBUG",
	[CO_LOG_INFO]     = "INFO",
	[CO_LOG_NOTICE]   = "NOTICE",
	[CO_LOG_WARN]     = "WARN",
	[CO_LOG_ERROR]    = "ERROR",
	[CO_LOG_FATAL]    = "FATAL",
};

void __co_log(
	co_logger_t                   *logger,
	const char                    *func,
	int                            line,
	co_log_level_t                 level,
	const char                    *fmt,
	...
) {
	char date[128];
	char buf[2048];
	time_t t;
	struct tm gmt;
	va_list va;

	if (level < logger->log_level)
		return;

	va_start(va, fmt);
	vsnprintf(buf, 2048, fmt, va);
	va_end(va);

	time(&t);
	gmtime_r(&t, &gmt);
	strftime(date, 128, "%Y-%m-%dT%H:%M:%SZ", &gmt);

	printf("[%s] %s:%d: [%s] %s\n",
		date, func, line, log_level_names[level], buf);
}

void co_log_level(
	co_context_t                  *ctx,
	co_logger_t                   *logger,
	co_log_level_t                 level
) {
	if (logger == NULL)
		logger = &ctx->log;
	logger->log_level = level;
}

co_logger_t *co_logger(
	co_context_t                  *ctx,
	co_logger_t                   *inherit
) {
	co_logger_t *logger = calloc(1, sizeof(*logger));

	if (inherit == NULL)
		inherit = &ctx->log;
	logger->inherit = inherit;
	logger->inherit->refcount++;
	logger->log_level = inherit->log_level;
	logger->refcount = 1;

	return logger;
}

void co_logger_close(
	co_context_t                  *ctx,
	co_logger_t                   *logger
) {
	logger->inherit->refcount--;
	if (logger->inherit->refcount <= 0)
		co_logger_close(ctx, logger);
	logger->refcount--;
	if (logger->refcount <= 0)
		free(logger);
}

ssize_t co_fprintf(
	co_context_t                  *ctx,
	co_file_t                     *file,
	const char                    *fmt,
	...
) {
	char buf[8192];
	va_list va;
	int sz;
	ssize_t wsize;

	va_start(va, fmt);
	sz = vsnprintf(buf, 8192, fmt, va);
	va_end(va);

	if (co_write(ctx, file, buf, sz, &wsize) < 0)
		return -1;

	return wsize;
}

co_context_t *co_init(void) {
	co_context_t *ctx;

	ctx = calloc(1, sizeof(*ctx));
	ctx->threads = thread_context_new();
	ctx->new_thread = NULL;
	ctx->files = NULL;
	ctx->log.inherit = NULL;
	ctx->log.log_level = CO_LOG_NOTICE;

	event_init();

	co_trace(&ctx->log, "co context initialized");

	return ctx;
}

static thread_t *unwait(co_context_t *ctx, int fd) {
	co_file_t *f;

	for (f=ctx->files; f; f=f->next) {
		if (f->fd == fd) {
			thread_t *t = f->waiting;
			co_trace(&ctx->log, "  fd=%d -> t=%p", fd, t);
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
	struct timeval now, fire, timeout;
	co_timer_t *timer;

	co_trace(&ctx->log, "poll...");

	if (ctx->num_threads == 0) {
		co_debug(&ctx->log, "no threads left; stopping");
		thread_context_stop(threads);
		return NULL;
	}

	if (ctx->new_thread) {
		struct new_thread *next = ctx->new_thread->next;
		res = ctx->new_thread->t;
		ctx->new_thread = next;
		co_debug(&ctx->log, "starting newly spawned thread");
		return res;
	}

	gettimeofday(&now, NULL);

	if (ctx->timers && !timercmp(&ctx->timers->fire, &now, >)) {
		/* fire timer */
		timer = ctx->timers;
		res = timer->waiting;
		ctx->timers = timer->next;
		free(timer);
		co_debug(&ctx->log, "firing timer for thread=%p", res);
		return res;
	}

	/* limit loop delay to 1 second */
	fire.tv_sec   = now.tv_sec + 1;
	fire.tv_usec  = now.tv_usec;

	if (ctx->timers && timercmp(&ctx->timers->fire, &fire, <)) {
		fire.tv_sec  = ctx->timers->fire.tv_sec;
		fire.tv_usec = ctx->timers->fire.tv_usec;
	}

	timersub(&fire, &now, &timeout);

	if (event_poll(&evt, &timeout)) {
		switch (evt.tag) {
		case EVENT_NOTHING:
			co_trace(&ctx->log, "no events ready");
			break;
		case EVENT_FD_CAN_READ:
		case EVENT_FD_CAN_WRITE:
			co_trace(&ctx->log, "fd=%d ready for IO", evt.v.fd);
			return unwait(ctx, evt.v.fd);
		}
	}

	/* don't check timers. we'll get the fired timer on the next loop */

	return NULL;
}

static void do_nothing(int unused) { }

void co_run(
	co_context_t                  *ctx,
	co_thread_fn                  *start,
	void                          *user
) {
	co_trace(&ctx->log, "installing dummy SIGPIPE handler");
	signal(SIGPIPE, do_nothing);

	co_trace(&ctx->log, "spawning start thread");
	co_spawn(ctx, start, user);

	co_debug(&ctx->log, "entering thread run loop");
	thread_context_run(ctx->threads, thread_poller, ctx);
}
