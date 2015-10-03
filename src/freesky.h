#ifndef FREESKY_H
#define FREESKY_H

#ifdef __cplusplus
extern "C" {
#endif


typedef int freesky_gpio_pin;
// A GPIO pin number as numbered by the linux kernel's GPIO subsystem.
// For example, if this value is 22, then libfreesky will interact with
// `/sys/class/gpio/gpio22`.

struct _freesky_device;
typedef struct _freesky_device freesky_device;
// Opaque struct holding device handles and such.

int freesky_open(freesky_device **pdev, const char* i2c_device, freesky_gpio_pin reset_pin, freesky_gpio_pin xfer_pin);
// Opens a Skywriter device with the specified i2c_device path,
// reset_pin, and xfer_pin (in terms of board coordinates), and
// assigns the newly-opened device to *dev.
//
// Returns 0 on success, < 0 on error.

int freesky_close(freesky_device *dev);
// Closes a device that is currently open.
//
// Returns 0 on success, < 0 on error.

void freesky_set_userdata(freesky_device *dev, void *userdata);
// Attach a pointer to the device, to allow callbacks a way to access other
// data.

void *freesky_get_userdata(freesky_device *dev);
// Return the data previously set for this device with freesky_set_userdata()


//////// Event callbacks

// There will probably be a number of different callbacks for different events
// reported from the Skywriter.  Add API for them here.
// I could see either an enum with the callback types and a single callback, or
// a bunch of callback functions that can be set separately.  The latter seems
// more type-safe.

typedef void (*freesky_callback)(freesky_device *dev);
// Typedef for various user callbacks.
// TODO: figure out all the callbacks we want to support, and also pass the
// callback the data in question?

void freesky_set_callback(freesky_device *dev, freesky_callback callback);
// TODO: figure out if this is the right API


//////// Event loop integration

// This needs a couple of things:
// * Some way for the client to tell freesky to ask the device for data
// * Some way for the 

int freesky_process_events(freesky_device *dev);
// Runs the event loop a single time.  Intended to be used with polling.
//
// Returns positive on success if more events remain to be processed,
// 0 on success if all events are processed, and negative on error.


#ifdef __cplusplus
}
#endif

#endif /* FREESKY_H */
