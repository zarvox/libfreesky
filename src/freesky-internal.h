#ifndef FREESKY_INTERNAL_H
#define FREESKY_INTERNAL_H

#include "freesky.h"

struct _freesky_device {
    // All the state that a device handle needs to track.
    // Add fields as appropriate.
    int i2c_fd;
    freesky_gpio_pin reset_pin;
    freesky_gpio_pin xfer_pin;
    freesky_callback callback;
    void* userdata;
    bool ok;
};

#endif // FREESKY_INTERNAL_H
