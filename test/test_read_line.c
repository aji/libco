#include <co.h>
#include <stdio.h>
#include <string.h>

static void main_thread(co_context_t *co, void *user) {
	co_file_t *peer;
	const char *request =
		"GET / HTTP/1.0\r\n"
		"Host: google.com\r\n"
		"Connection: close\r\n"
		"User-Agent: libco/0.1\r\n"
		"\r\n";
	char buf[2048];
	ssize_t rsize;

	printf("connecting...\n");
	peer = co_connect_tcp(co, "google.com", 80);

	if (peer == NULL) {
		printf("connect failed :(\n");
		return;
	}

	printf("connected! sending request...\n");
	co_write(co, peer, request, strlen(request), NULL);
	printf("request sent! reading response, a line at a time...\n");

	while (co_read_line(co, peer, buf, 2048)) {
		printf("line: %s\n", buf);
	}

	printf("end of response. bye!\n");
}

int main(int argc, char *argv[]) {
	co_context_t *co;

	co = co_init();
	co_run(co, main_thread, NULL);
}
