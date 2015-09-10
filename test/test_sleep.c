#include <co.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct delay {
	const char *name;
	int iters;
	int usecs;
};

static void sleeper_thread(co_context_t *co, void *_delay) {
	struct delay *delay = _delay;
	int i;

	for (i=0; i<delay->iters; i++) {
		printf("%s sleep...\n", delay->name);
		co_usleep(co, delay->usecs);
	}

	printf("%s done!\n", delay->name);
}

static void main_thread(co_context_t *co, void *user) {
	struct delay *one, *two;

	one = calloc(1, sizeof(struct delay));
	one->name = "one";
	one->iters = 15;
	one->usecs = 200000;

	two = calloc(1, sizeof(struct delay));
	two->name = "two";
	two->iters = 5;
	two->usecs = 500000;

	printf("spawning threads...\n");
	co_spawn(co, sleeper_thread, one);
	co_spawn(co, sleeper_thread, two);
	printf("done\n");
}

int main(int argc, char *argv[]) {
	co_context_t *co;

	co = co_init();
	co_run(co, main_thread, NULL);
}
