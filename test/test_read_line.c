#include <co.h>
#include <stdio.h>
#include <string.h>

static void main_thread(co_context_t *co, void *user) {
	co_file_t *peer;
	co_logger_t *log;
	const char *request =
		"GET / HTTP/1.0\r\n"
		"Host: google.com\r\n"
		"Connection: close\r\n"
		"User-Agent: libco/0.1\r\n"
		"\r\n";
	char buf[2048];
	ssize_t rsize;

	log = co_logger(co, NULL);
	co_log_level(co, log, CO_LOG_INFO);

	co_info(log, "connecting...");
	peer = co_connect_tcp(co, "google.com", 80);

	if (peer == NULL) {
		co_error(log, "connect failed :(");
		return;
	}

	co_info(log, "connected! sending request...");
	co_write(co, peer, request, strlen(request), NULL);
	co_info(log, "request sent! reading response, a line at a time...");

	while (co_read_line(co, peer, buf, 2048)) {
		co_info(log, "line: %s$", buf);
	}

	co_info(log, "end of response. bye!");
}

int main(int argc, char *argv[]) {
	co_context_t *co;

	co = co_init();
	co_log_level(co, NULL, CO_LOG_DEBUG);
	co_run(co, main_thread, NULL);
}
