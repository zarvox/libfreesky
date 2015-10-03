#include "freesky.h"
#include "freesky-internal.h"

#include <stdlib.h> // for malloc(), free()
#include <string.h> // for memset()

// for open()
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <linux/ioctl.h> // for ioctl()
#include <errno.h>

#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include "i2c-helper.h"

#define UNUSED(X) (void)(X);

int freesky_open(freesky_device **pdev, const char* i2c_device, freesky_gpio_pin reset_pin, freesky_gpio_pin xfer_pin) {
	// Initialize a new struct on the heap.
	*pdev = (freesky_device*)malloc(sizeof(freesky_device));
	freesky_device* dev = *pdev;
	memset(dev, 0, sizeof(*dev));

	// Set up the pins.
	dev->reset_pin = reset_pin;
	dev->xfer_pin = xfer_pin;

	// Open the i2c device.
	dev->i2c_fd = open(i2c_device, O_RDWR);
	if (dev->i2c_fd < 0) {
		return -errno;
	}

	// Set the slave address.
	if (ioctl(dev->i2c_fd, I2C_SLAVE, SKYWRITER_ADDR) < 0) {
		return -errno;
	}

	return 0;
}

int freesky_close(freesky_device *dev) {
	free(dev);
	return 0;
}

void freesky_set_userdata(freesky_device *dev, void *userdata) {
	dev->userdata = userdata;
}

void *freesky_get_userdata(freesky_device *dev) {
	return dev->userdata;
}

void freesky_set_callback(freesky_device *dev, freesky_callback callback) {
	dev->callback = callback;
}

void freesky_process_events(freesky_device *dev) {
	UNUSED(dev);
	// noop for now
}
