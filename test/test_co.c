#include <co.h>
#include <stdio.h>

static void my_thread(co_context_t *co, void *user) {
	printf("hello from my thread!\n");
}

int main(int argc, char *argv[]) {
	co_context_t *co;

	co = co_init();
	co_run(co, my_thread, NULL);
}
