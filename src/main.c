#include "freesky.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>

#define LOG(...) fprintf(stderr, __VA_ARGS__)
#define DIE(...) do { LOG(__VA_ARGS__); exit(1); } while(0);
#define UNUSED(x) (void)(x)

int quit = 0;

void handle_sigint(int sig) {
	UNUSED(sig);
	quit = 1;
}

int main(int argc, char** argv) {
	// unused
	UNUSED(argc);
	UNUSED(argv);

	// Set up signal handler, so we can ^C cleanly
	struct sigaction sa;
	sa.sa_handler = &handle_sigint;
	sigaction(SIGINT, &sa, NULL);

	int res;
	freesky_device* dev = NULL;
	int test = 42;

	char* devicepath = "/dev/i2c-1";
	res = freesky_open(&dev, devicepath, 4, 5);
	if (res < 0) {
		DIE("couldn't open device\n");
	}
	LOG("opened skywriter\n");

	freesky_set_userdata(dev, &test);

	int* testref = (int*)freesky_get_userdata(dev);
	LOG("%d\n", *testref);
	LOG("roundtripped userdata\n");
	
	while(1) {
		if (quit) break;
		int res = freesky_process_events(dev);
		if (res < 0) break;
	}

	freesky_close(dev);
	LOG("closed skywriter\n");
	return 0;
}
