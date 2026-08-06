#pragma once

#include "device.hpp"
#include "logger.hpp"

namespace iris::usb::mouse {

enum Button : int {
    LEFT,
    RIGHT,
    MIDDLE
};

void create(device::Device* dev);

// Frontend input hooks: accumulate relative motion / set a button state.
void move(device::Device* dev, int dx, int dy, int dz);
void button(device::Device* dev, int button, int pressed);

}
