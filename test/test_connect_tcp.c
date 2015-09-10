#include <co.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BUFSIZE 2048

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
		"User-Agent: libco/0.0\r\n"
		"Accept: */*\r\n"
		"Connection: close\r\n"
		"\r\n";
	char buf[BUFSIZE];
	ssize_t rsize;

	snprintf(buf, BUFSIZE, request_fmt, conn->host);

	printf("%s: connecting to %s\n", conn->name, conn->host);
	peer = co_connect_tcp(co, conn->host, 80);

	if (peer == NULL) {
		printf("%s: connect failed :(\n", conn->name);
		return;
	}

	printf("%s: connected! sending request...\n", conn->name);
	co_write(co, peer, buf, strlen(buf), NULL);
	printf("%s: request sent!\n", conn->name);
	printf("%s: reading response...\n", conn->name);

	do {
		memset(buf, 0, sizeof(buf));
		co_read(co, peer, buf, BUFSIZE, &rsize);
		printf("%s: got %d bytes\n", conn->name, rsize);
	} while (rsize);

	printf("%s: bye!\n", conn->name);

	free(conn);
}

static void main_thread(co_context_t *co, void *user) {
	struct connector *google, *interlinked, *nowhere, *wiki, *amazon;

	google = calloc(1, sizeof(*google));
	google->host = "www.google.com";
	google->name = "google";

	interlinked = calloc(1, sizeof(*interlinked));
	interlinked->host = "interlinked.me";
	interlinked->name = "interlinked";

	nowhere = calloc(1, sizeof(*nowhere));
	nowhere->host = "nonexistent.badtld";
	nowhere->name = "nowhere";

	wiki = calloc(1, sizeof(*wiki));
	wiki->host = "en.wikipedia.org";
	wiki->name = "wikipedia";

	amazon = calloc(1, sizeof(*amazon));
	amazon->host = "www.amazon.com";
	amazon->name = "amazon";

	printf("spawning threads...\n");
	co_spawn(co, connector_thread, google);
	co_spawn(co, connector_thread, interlinked);
	co_spawn(co, connector_thread, nowhere);
	co_spawn(co, connector_thread, wiki);
	co_spawn(co, connector_thread, amazon);
	printf("done\n");
}

int main(int argc, char *argv[]) {
	co_context_t *co;

	co = co_init();
	co_run(co, main_thread, NULL);
}
