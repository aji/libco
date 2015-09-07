#include <stdio.h>

#include "../src/thread.h"

static thread_t *inside_thread;
static volatile int counter = 0;

static void inside(thread_context_t *ctx, void *_unused) {
	int i;
	for (i=0; ; i++) {
		printf("iteration %d!\n", i+1);
		thread_defer_self(ctx);
	}
}

static thread_t *poller(thread_context_t *ctx, void *_unused) {
	if (counter < 3) {
		counter ++;
		return inside_thread;
	} else {
		thread_context_stop(ctx);
		return NULL;
	}
}

int main(int argc, char *argv[]) {
	thread_context_t *ctx = thread_context_new();
	inside_thread = thread_create(ctx, inside, NULL);
	printf("starting thread runtime!\n");
	thread_context_run(ctx, poller, NULL);
	printf("thread runtime finished!\n");
}
