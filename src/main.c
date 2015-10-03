#include "freesky.h"
#include <stdlib.h>
#include <stdio.h>

#define LOG(...) fprintf(stderr, __VA_ARGS__)
#define DIE(...) do { LOG(__VA_ARGS__); exit(1); } while(0);
#define UNUSED(x) (void)(x)

int main(int argc, char** argv) {
	// unused
	UNUSED(argc);
	UNUSED(argv);

	int res;
	freesky_device* dev = NULL;
	int test = 42;

	char* devicepath = "/dev/i2c-1";
	res = freesky_open(&dev, devicepath, 48, 51);
	if (res < 0) {
		DIE("couldn't open device\n");
	}
	LOG("opened skywriter\n");

	freesky_set_userdata(dev, &test);

	int* testref = (int*)freesky_get_userdata(dev);
	LOG("%d\n", *testref);
	LOG("roundtripped userdata\n");
	
	freesky_close(dev);
	LOG("closed skywriter\n");
	return 0;
}
