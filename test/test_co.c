#include <co.h>
#include <stdio.h>

static void my_thread(co_context_t *co, void *user) {
	co_file_t *in = co_open(co, "/dev/stdin", CO_RDONLY, 0666);
	char buf[4];
	ssize_t rsize;

	printf("hello from my thread!\n");

	do {
		memset(buf, 0, sizeof(buf));
		co_read(co, in, &buf, 4, &rsize);
		printf("got %d bytes: %.4s\n", rsize, buf);
	} while (rsize);
}

int main(int argc, char *argv[]) {
	co_context_t *co;

	co = co_init();
	co_run(co, my_thread, NULL);
}
