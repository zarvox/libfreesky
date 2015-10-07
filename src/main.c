#include "freesky.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <string.h>
#include <sys/time.h>

#define LOG(...) fprintf(stderr, __VA_ARGS__)
#define DIE(...) do { LOG(__VA_ARGS__); exit(1); } while(0);
#define UNUSED(x) (void)(x)

int quit = 0;

void handle_sigint(int sig) {
	// Catch a ^C on the terminal and shut down cleanly.
	UNUSED(sig);
	quit = 1;
}

typedef struct {
	freesky_point point; // X/Y/Z coordinates of last tracked point
	struct timeval timestamp;    // Timestamp when this point was tracked.
} buffer_t;

typedef struct {
	pthread_mutex_t lock;   // guards other members
	//pthread_cond_t condvar; // for waking up other thread if contended
	freesky_device *dev;
	buffer_t buffer;
} device_thread_state;

#define MAX_DEVICES 2
typedef struct {
	device_thread_state* devices[MAX_DEVICES];
} thread_state_t;

device_thread_state* create_thread_state(freesky_device *dev) {
	device_thread_state* dev_state = malloc(sizeof(device_thread_state));
	memset(dev_state, 0, sizeof(*dev_state));
	dev_state->dev = dev;
	LOG("dev: %p\n", dev_state->dev);
	pthread_mutex_init(&dev_state->lock, NULL);
	//pthread_cond_init(&dev_state->condvar, NULL);
	return dev_state;
}

freesky_device* destroy_thread_state(device_thread_state* dev_state) {
	//pthread_cond_destroy(&dev_state->condvar);
	pthread_mutex_destroy(&dev_state->lock);
	freesky_device* dev = dev_state->dev;
	free(dev_state);
	return dev;
}

void update_buffer_callback(freesky_device *dev, freesky_point point) {
	device_thread_state* ctx = (device_thread_state*)freesky_get_userdata(dev);
	pthread_mutex_lock(&ctx->lock);
	ctx->buffer.point.x = point.x;
	ctx->buffer.point.y = point.y;
	ctx->buffer.point.z = point.z;
	gettimeofday(&ctx->buffer.timestamp, NULL);
	//pthread_cond_signal(&ctx->condvar);
	pthread_mutex_unlock(&ctx->lock);
	//LOG("[bg  ] x: %f\ty: %f\tz: %f\n", point.x, point.y, point.z);
}

static void *thread_func(void *arg)
{
	// Spawning thread will set up a thread_state_t with the devices of interest.
	thread_state_t* state = (thread_state_t*)(arg);

	// Count devices, until the first NULL device.  These are the ones we'll poll.
	// Set up callbacks and userdata for each of the devices.
	int devices = 0;
	for (int i = 0 ; i < MAX_DEVICES ; i++) {
		if (state->devices[i] != NULL) {
			pthread_mutex_lock(&state->devices[i]->lock);
			freesky_device* dev = state->devices[i]->dev;
			freesky_set_userdata(dev, state->devices[i]);
			freesky_set_callback(dev, update_buffer_callback);
			//pthread_cond_signal(&state->devices[i]->condvar);
			pthread_mutex_unlock(&state->devices[i]->lock);
			devices++;
		} else {
			break;
		}
	}

	// Run event loop for each device.
	while (!quit) {
		for(int i = 0 ; i < devices; i++) {
			int res = freesky_process_events(state->devices[i]->dev);
			if (res < 0) {
				LOG("[bg  ] freesky_process_events() failed: %d\n", res);
				break;
			}
			if (quit) { 
				LOG("[bg  ] shutdown requested\n");
				break;
			}
		}
	}
	return NULL;
}

int main(int argc, char** argv) {
	// unused
	UNUSED(argc);
	UNUSED(argv);

	// Set up signal handler, so we can ^C cleanly
	struct sigaction sa;
	sa.sa_handler = &handle_sigint;
	sigaction(SIGINT, &sa, NULL);

	// Open devices.
	int res;
	freesky_device* dev = NULL;
	char* devicepath = "/dev/i2c-1";
	res = freesky_open(&dev, devicepath, 4, 5);
	if (res < 0) {
		DIE("couldn't open device\n");
	}
	LOG("opened skywriter\n");

	// Set up state to share with background thread.
	thread_state_t thread_state;
	memset(&thread_state, 0, sizeof(thread_state));
	// If you wanted to support more than one device, you'd simply open it above and set it to the
	// next thread_state.devices[] entry.
	thread_state.devices[0] = create_thread_state(dev);

	// Start background thread to poll skywriter.
	pthread_t freesky_thread;
	pthread_create(&freesky_thread, NULL, thread_func, &thread_state);
	LOG("[main] spawned background thread\n");

	// Application mainloop.
	while(1) {
		if (quit) break;

		// Here we grab the most recent sample.
		buffer_t last_sample;
		// While holding the buffer lock, copy the last tracked point.
		pthread_mutex_lock(&thread_state.devices[0]->lock);
		last_sample.point.x = thread_state.devices[0]->buffer.point.x;
		last_sample.point.y = thread_state.devices[0]->buffer.point.y;
		last_sample.point.z = thread_state.devices[0]->buffer.point.z;
		last_sample.timestamp = thread_state.devices[0]->buffer.timestamp;
		//pthread_cond_signal(&thread_state.devices[0]->condvar);
		pthread_mutex_unlock(&thread_state.devices[0]->lock);

		// Do some work based on that most recent sample.
		LOG("[main] x: %f\ty: %f\tz: %f\ttime: %ld.%06ld\n",
				last_sample.point.x,
				last_sample.point.y,
				last_sample.point.z,
				last_sample.timestamp.tv_sec,
				last_sample.timestamp.tv_usec);
		usleep(16667); // render a frame or something
	}

	pthread_join(freesky_thread, NULL);
	destroy_thread_state(thread_state.devices[0]);

	freesky_close(dev);
	LOG("closed skywriter\n");
	return 0;
}
