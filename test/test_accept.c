#include <co.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void main_thread(co_context_t *co, void *user) {
	co_file_t *listener;
	int i;

	printf("attempting to bind to port 4321\n");
	listener = co_bind_tcp6(co, "::", 4321, 5);

	if (!listener) {
		printf("bind failed! :(\n");
		return;
	}

	printf("will accept *5* clients\n");
	for (i=0; i<5; i++) {
		co_file_t *peer = co_accept(co, listener, NULL, 0, NULL);

		printf("got client #%d!\n", i+1);
		co_write(co, peer, "greetings\n", 10, NULL);
		co_close(co, peer);
	}

	printf("that's 5, bye!\n");
}

int main(int argc, char *argv[]) {
	co_context_t *co;

	co = co_init();
	co_log_level(co, NULL, CO_LOG_DEBUG);
	co_run(co, main_thread, NULL);
}
