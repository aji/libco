#include <co.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct connector {
	const char *host;
	const char *name;
};

static void connector_thread(co_context_t *co, void *_conn) {
	co_file_t *peer;
	struct connector *conn = _conn;
	const char *request_fmt =
		"GET / HTTP/1.0\r\n"
		"Host: %s\r\n"
		"Connection: close\r\n"
		"User-Agent: libco/0.1\r\n"
		"\r\n";
	char buf[65536];
	ssize_t rsize;

	snprintf(buf, 65536, request_fmt, conn->host);

	printf("%s: connecting to %s\n", conn->name, conn->host);
	peer = co_connect_tcp(co, conn->host, 80);

	if (peer == NULL) {
		printf("%s: connect failed :(\n", conn->name);
		return;
	}

	printf("%s: connected! sending request...\n", conn->name);
	co_write(co, peer, buf, strlen(buf), NULL);
	printf("%s: request sent! reading response...\n", conn->name);

	do {
		memset(buf, 0, sizeof(buf));
		co_read(co, peer, buf, 65536, &rsize);
		printf("%s: got %d bytes\n", conn->name, rsize);
	} while (rsize);

	printf("%s: bye!\n", conn->name);

	free(conn);
}

static void main_thread(co_context_t *co, void *user) {
	struct connector *google, *interlinked, *nowhere;

	google = calloc(1, sizeof(*google));
	google->host = "www.google.com";
	google->name = "google";

	interlinked = calloc(1, sizeof(*interlinked));
	interlinked->host = "interlinked.me";
	interlinked->name = "interlinked";

	nowhere = calloc(1, sizeof(*nowhere));
	nowhere->host = "nonexistent.badtld";
	nowhere->name = "nowhere";

	printf("spawning threads...\n");
	co_spawn(co, connector_thread, google);
	co_spawn(co, connector_thread, interlinked);
	co_spawn(co, connector_thread, nowhere);
	printf("done\n");
}

int main(int argc, char *argv[]) {
	co_context_t *co;

	co = co_init();
	co_run(co, main_thread, NULL);
}
