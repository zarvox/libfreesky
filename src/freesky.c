#include "freesky.h"
#include "freesky-internal.h"

#include <stdio.h> // for fprintf(3)
#include <stdlib.h> // for malloc(3), free(3)
#include <string.h> // for memset(3)

// for open(2)
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <unistd.h> // for write(2), close(2), usleep(3)

#include <linux/ioctl.h> // for ioctl(2)
#include <errno.h> // for errno(3)

#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include "i2c-helper.h"

#define LOG(...) fprintf(stderr, __VA_ARGS__)
#define UNUSED(X) (void)(X);
#define SYSFS_GPIO_DIR "/sys/class/gpio"

// ---- Internal helpers and utilities ----

typedef enum {
	GPIO_OUT,
	GPIO_IN
} gpio_direction;

typedef enum {
	GPIO_LOW,
	GPIO_HIGH,
} gpio_voltage;

static void export_gpio(freesky_gpio_pin pin) {
	// echo $pin > /sys/class/gpio/export
	int fd = open(SYSFS_GPIO_DIR "/export", O_WRONLY);
	char buf[12];
	memset(buf, 0, 12);
	int len = snprintf(buf, 11, "%d", pin);
	write(fd, buf, len);
	close(fd);
}

static void unexport_gpio(freesky_gpio_pin pin) {
	// echo $pin > /sys/class/gpio/unexport
	int fd = open(SYSFS_GPIO_DIR "/unexport", O_WRONLY);
	char buf[12];
	memset(buf, 0, 12);
	int len = snprintf(buf, 12, "%d", pin);
	write(fd, buf, len);
	close(fd);
}

static void configure_gpio(freesky_gpio_pin pin, gpio_direction direction, gpio_voltage voltage) {
	char buf[128];
	int fd;

	snprintf(buf, 128, SYSFS_GPIO_DIR "/gpio%d/direction", pin);
	fd = open(buf, O_WRONLY);
	switch(direction) {
		case GPIO_OUT: write(fd, "out", 3); break;
		case GPIO_IN: write(fd, "in", 2); break;
	}
	close(fd);

	snprintf(buf, 128, SYSFS_GPIO_DIR "/gpio%d/value", pin);
	fd = open(buf, O_WRONLY);
	switch(voltage) {
		case GPIO_LOW: write(fd, "0", 1); break;
		case GPIO_HIGH: write(fd, "1", 1); break;
	}
	close(fd);
}

static int xfer_ready(freesky_device* dev) {
	// Returns 1 if a transfer is ready, 0 if not ready, and -errno on error.
	if (lseek(dev->xfer_fd, 0, SEEK_SET) < 0) {
		return -errno;
	}
	char buf = '0';
	if (read(dev->xfer_fd, &buf, 1) != 1) {
		return -errno;
	}
	return (buf == '0') ? 1 : 0;
}

static int do_xfer(freesky_device* dev) {
	int retval = 0;
	// 1. Assert transfer line low to preserve data buffers.
	configure_gpio(dev->xfer_pin, GPIO_OUT, GPIO_LOW);
	// 2. Do block read
	__u8 buf[32];
	memset(buf, 0, 32);
	int32_t len = i2c_smbus_read_i2c_block_data(dev->i2c_fd, 0x00, 32, buf);
	if (len < 0) {
		perror("i2c_smbus_read_block_data");
		retval = -errno;
	}
	if (len > 0) {
	// 3. TODO: decode bytes and call appropriate callback
	//    for now, just log bytes to the screen
		for (int32_t i = 0 ; i < len ; i++ ) {
			LOG("%02X ", buf[i]);
		}
		LOG("\n");
	}
	// 4. Float transfer line high.
	configure_gpio(dev->xfer_pin, GPIO_IN, GPIO_HIGH);
	// Wait 200 usec.
	// TODO: record the current time and block on the next process_events call if too soon instead?
	// might require change in the process_events return value
	usleep(200);
	return retval;
}

// ---- Public API functions begin here. ----

int freesky_open(freesky_device **pdev, const char* i2c_device, freesky_gpio_pin reset_pin, freesky_gpio_pin xfer_pin) {
	// Initialize a new struct on the heap.
	*pdev = (freesky_device*)malloc(sizeof(freesky_device));
	freesky_device* dev = *pdev;
	memset(dev, 0, sizeof(*dev));

	// Set up the GPIO pins.
	dev->reset_pin = reset_pin;
	dev->xfer_pin = xfer_pin;
	export_gpio(dev->reset_pin);
	export_gpio(dev->xfer_pin);
	configure_gpio(dev->reset_pin, GPIO_OUT, GPIO_HIGH);
	configure_gpio(dev->xfer_pin, GPIO_IN, GPIO_HIGH);

	char buf[128];
	snprintf(buf, 128, SYSFS_GPIO_DIR "/gpio%d/value", xfer_pin);
	dev->xfer_fd = open(buf, O_RDONLY);
	
	// Reset the skywriter, by sending a 100msec pulse low on the reset pin
	LOG("Resetting skywriter...");
	configure_gpio(dev->reset_pin, GPIO_OUT, GPIO_LOW);
	usleep(100000);
	
	// Then wait for the skywriter to start up again.  The datasheet claims 200msec for this; we'll
	// give it 300, just to be generous.
	configure_gpio(dev->reset_pin, GPIO_OUT, GPIO_HIGH);
	usleep(300000);
	LOG("done.\n");

	// Open the i2c device.
	dev->i2c_fd = open(i2c_device, O_RDWR);
	if (dev->i2c_fd < 0) {
		perror("couldn't open i2c device");
		return -errno;
	}

	// Set the slave address.
	if (ioctl(dev->i2c_fd, I2C_SLAVE, SKYWRITER_ADDR) < 0) {
		perror("couldn't set i2c slave address");
		return -errno;
	}

	// Tell the skywriter to start streaming data.
	__u8 magic_command = 0xa1;
	__u8 magic_values[4] = { 0b00000000, 0b00011111, 0b00000000, 0b00011111 };
	if (i2c_smbus_write_i2c_block_data(dev->i2c_fd, magic_command, 4, magic_values) < 0) {
		perror("couldn't do i2c block write");
		return -errno;
	}
	return 0;
}

int freesky_close(freesky_device *dev) {
	if (dev->i2c_fd) {
		close(dev->i2c_fd);
		dev->i2c_fd = -1;
	}

	configure_gpio(dev->reset_pin, GPIO_IN, GPIO_HIGH);
	configure_gpio(dev->xfer_pin, GPIO_IN, GPIO_HIGH);
	unexport_gpio(dev->reset_pin);
	unexport_gpio(dev->xfer_pin);

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

int freesky_process_events(freesky_device *dev) {
	// TODO: check if 200usec have passed since we last released xfer_pin, and if not, block
	int res = xfer_ready(dev);
	if (res <= 0) {
		return res;
	}
	return do_xfer(dev);
}
