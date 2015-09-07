/* test_e_select -- tests select backend specifically */
/* Copyright (C) 2015 Alex Iadicicco */

#include "../src/event.h"

#include <stdio.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
	event_polled_t result;
	char buf[4];

	event_init();
	event_fd_want_read(0);

	for (;;) {
		if (!event_poll(&result)) {
			printf("sorry nothing!\n");
			continue;
		}

		switch (result.tag) {
		case EVENT_FD_CAN_READ:
			printf("fd=%d can read! ", result.v.fd);
			printf("asked for 4 bytes, got %d\n",
				read(result.v.fd, &buf, 4));
			event_fd_want_read(result.v.fd);
			break;
		}
	}
}
