#include <co.h>
#include <stdio.h>
#include <string.h>

static void stdin_thread(co_context_t *co, void *user) {
	co_file_t *in = co_open(co, "/dev/stdin", CO_RDONLY, 0666);
	char buf[4];
	ssize_t rsize;

	printf("hello from stdin_thread!\n");

	do {
		memset(buf, 0, sizeof(buf));
		co_read(co, in, &buf, 4, &rsize);
		printf("got %d bytes: %.4s\n", rsize, buf);
	} while (rsize);

	printf("stdin_thread done!\n");
}

static void google_thread(co_context_t *co, void *user) {
	co_file_t *goog;
	const char *request =
		"GET / HTTP/1.0\r\n"
		"Host: google.com\r\n"
		"Connection: close\r\n"
		"User-Agent: libco/0.1\r\n"
		"\r\n";
	char buf[2048];
	ssize_t rsize;

	printf("hello from google_thread! connecting...\n");
	goog = co_connect_tcp(co, "google.com", 80);

	if (goog == NULL) {
		printf("connect failed :(\n");
		return;
	}

	printf("connected to google! sending request...\n");
	co_write(co, goog, request, strlen(request), NULL);
	printf("request sent! reading response...\n");

	do {
		memset(buf, 0, sizeof(buf));
		co_read(co, goog, &buf, 2048, &rsize);
		printf("got %d bytes from google\n", rsize, buf);
	} while (rsize);
}

static void main_thread(co_context_t *co, void *user) {
	co_spawn(co, google_thread, NULL);
	co_spawn(co, stdin_thread, NULL);
}

int main(int argc, char *argv[]) {
	co_context_t *co;

	co = co_init();
	co_run(co, main_thread, NULL);
}
