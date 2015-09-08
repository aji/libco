/* ircbot.c -- libco IRC bot demo */
/* Copyright (C) 2015 Alex Iadicicco */

#include <co.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#define MAX_PARAMS 32

struct parsed {
	const char *source;
	const char *source_extra;

	const char *verb;

	const char *parv[MAX_PARAMS];
	int parc;
};

static void p_space(char **s) {
	while (**s && isspace(**s)) (*s)++;
}

static char *p_nonspace(char **s) {
	char *result = *s;
	while (**s && !isspace(**s)) (*s)++;
	if (**s) *(*s)++ = '\0';
	return result;
}

static void p_source(char **s, struct parsed *m) {
	m->source = *s;
	while (**s && !isspace(**s)) {
		if (**s != '!') { (*s)++; continue; }
		*(*s)++ = '\0';
		m->source_extra = *s;
		p_nonspace(s);
		return;
	}
	if (**s) *(*s)++ = '\0';
}

static int parse_message(char *s, struct parsed *m) {
	memset(m, 0, sizeof(*m));
	p_space(&s);
	if (*s == ':') { s++; p_source(&s, m); p_space(&s); }
	m->verb = p_nonspace(&s);
	p_space(&s);
	while (*s) {
		if (*s == ':') { s++; m->parv[m->parc++] = s; break; }
		m->parv[m->parc++] = p_nonspace(&s);
		p_space(&s);
	}
	return 0;
}

struct bot {
	/* configuration: */
	co_logger_t *root_logger;
	const char *host;
	unsigned short port;
	const char *channels;

	/* state: */
	bool running;
	co_logger_t *log;
	co_file_t *conn;
};

static void bot_privmsg_handler(
	co_context_t *co,
	struct bot *b,
	struct parsed *msg
) {
	co_info(b->log, "%s: <%s> %s", msg->parv[0], msg->source, msg->parv[1]);
	if (!strcmp(msg->parv[1], ".quit"))
		b->running = false;
}

static void bot_irc_handler(
	co_context_t *co,
	struct bot *b,
	struct parsed *msg
) {
	if (!strcmp(msg->verb, "PING")) {
		co_fprintf(co, b->conn, "PONG %s\r\n", msg->parv[0]);
	} else if (!strcmp(msg->verb, "001")) {
		co_fprintf(co, b->conn, "JOIN %s\r\n", b->channels);
	} else if (!strcmp(msg->verb, "NOTICE")) {
		co_info(b->log, "%s: -%s- %s", msg->parv[0],
			msg->source, msg->parv[1]);
	} else if (!strcmp(msg->verb, "PRIVMSG")) {
		bot_privmsg_handler(co, b, msg);
	}
}

static void bot_thread(co_context_t *co, void *_bot) {
	struct bot *b = _bot;
	struct parsed msg;
	char line[1024];
	int i;

	b->log = co_logger(co, b->root_logger);

	co_info(b->log, "connecting to %s:%d...", b->host, b->port);
	b->conn = co_connect_tcp(co, b->host, b->port);

	if (!b->conn) {
		co_error(b->log, "connection failed! exiting");
		return;
	}

	co_info(b->log, "connected! registering...");
	co_fprintf(co, b->conn, "NICK cobot\r\n");
	co_fprintf(co, b->conn, "USER co * 0 :cobot\r\n");

	b->running = true;

	while (b->running && co_read_line(co, b->conn, line, 1024)) {
		co_debug(b->log, " <- %s", line);
		if (parse_message(line, &msg) < 0)
			continue;

		co_trace(b->log, "source=%s$%s$", msg.source, msg.source_extra);
		co_trace(b->log, "verb=%s$ parc=%d parv:", msg.verb, msg.parc);
		for (i=0; i<msg.parc; i++)
			co_trace(b->log, "   %s$", msg.parv[i]);

		bot_irc_handler(co, b, &msg);
	}

	co_info(b->log, "bot finished!");
	co_close(co, b->conn);
}

static void main_thread(co_context_t *co, void *unused) {
	co_logger_t *root_logger = co_logger(co, NULL);

	struct bot *bot1 = calloc(1, sizeof(*bot1));
	struct bot *bot2 = calloc(1, sizeof(*bot2));

	bot1->root_logger = root_logger;
	bot1->host = "irc.interlinked.me";
	bot1->port = 6667;
	bot1->channels = "#test";

	bot2->root_logger = root_logger;
	bot2->host = "irc.ponychat.net";
	bot2->port = 6667;
	bot2->channels = "#test";

	co_spawn(co, bot_thread, bot1);
	co_spawn(co, bot_thread, bot2);
}

int main(int argc, char *argv[]) {
	co_context_t *co;

	co = co_init();
	co_log_level(co, NULL, CO_LOG_DEBUG);

	co_run(co, main_thread, NULL);
}
