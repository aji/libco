/* co.h -- libco header */
/* Copyright (C) 2015 Alex Iadicicco */

#ifndef __INC_CO_H__
#define __INC_CO_H__

#include <stdbool.h>
#include <unistd.h>

typedef int                            co_err_t;

typedef enum co_open_type              co_open_type_t;
typedef enum co_log_level              co_log_level_t;

typedef struct co_context              co_context_t;
typedef struct co_file                 co_file_t;
typedef struct co_logger               co_logger_t;

enum co_open_type {
	CO_RDONLY,
	CO_WRONLY,
	CO_RDWR,
	CO_APPEND
};

enum co_log_level {
	CO_LOG_TRACE    = 0,
	CO_LOG_DEBUG    = 1,
	CO_LOG_INFO     = 2,
	CO_LOG_NOTICE   = 3,
	CO_LOG_WARN     = 4,
	CO_LOG_ERROR    = 5,
	CO_LOG_FATAL    = 6
};

typedef void co_thread_fn(
	co_context_t                  *ctx,
	void                          *user
	);


/* THREAD OPERATIONS
   ========================================================================= */

extern co_err_t co_spawn(
	co_context_t                  *ctx,
	co_thread_fn                  *start,
	void                          *user
	);


/* IO OPERATIONS
   ========================================================================= */

extern co_err_t co_read(
	co_context_t                  *ctx,
	co_file_t                     *file,
	void                          *buf,
	size_t                         nbyte,
	ssize_t                       *rsize
	);

extern bool co_read_line(
	co_context_t                  *ctx,
	co_file_t                     *file,
	void                          *buf,
	size_t                         nbyte
	);

extern co_err_t co_write(
	co_context_t                  *ctx,
	co_file_t                     *file,
	const void                    *buf,
	size_t                         nbyte,
	ssize_t                       *wsize
	);

/* -- filesystem -- */

extern co_file_t *co_open(
	co_context_t                  *ctx,
	const char                    *path,
	co_open_type_t                 typ,
	unsigned                       mode
	);

extern void co_close(
	co_context_t                  *ctx,
	co_file_t                     *file
	);

/* -- sockets -- */

extern co_file_t *co_connect_tcp(
	co_context_t                  *ctx,
	const char                    *host,
	unsigned short                 port
	);

extern co_file_t *co_bind_tcp(
	co_context_t                  *ctx,
	const char                    *host,
	unsigned short                 port,
	int                            backlog
	);


/* CONVENIENCE AND HELPERS
   ========================================================================= */

extern void __co_log(
	co_logger_t                   *logger,
	const char                    *func,
	int                            line,
	co_log_level_t                 level,
	const char                    *fmt,
	...
	);

#define co_log(LOG, LEVEL, M...) \
	__co_log(LOG, __FUNCTION__, __LINE__, LEVEL, M)

#define co_trace(  LOG, M...)   co_log(LOG, CO_LOG_TRACE,   M)
#define co_debug(  LOG, M...)   co_log(LOG, CO_LOG_DEBUG,   M)
#define co_info(   LOG, M...)   co_log(LOG, CO_LOG_INFO,    M)
#define co_notice( LOG, M...)   co_log(LOG, CO_LOG_NOTICE,  M)
#define co_warn(   LOG, M...)   co_log(LOG, CO_LOG_WARN,    M)
#define co_error(  LOG, M...)   co_log(LOG, CO_LOG_ERROR,   M)
#define co_fatal(  LOG, M...)   co_log(LOG, CO_LOG_FATAL,   M)

extern void co_log_level(
	co_context_t                  *ctx,
	co_logger_t                   *logger,
	co_log_level_t                 level
	);

extern co_logger_t *co_logger(
	co_context_t                  *ctx,
	co_logger_t                   *inherit
	);

extern ssize_t co_fprintf(
	co_context_t                  *ctx,
	co_file_t                     *file,
	const char                    *fmt,
	...
	);

/* CONTEXT MANAGEMENT
   ========================================================================= */

extern co_context_t *co_init(void);

extern void co_run(
	co_context_t                  *ctx,
	co_thread_fn                  *start,
	void                          *user
	);

#endif
