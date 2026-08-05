#pragma once

#include "device.hpp"
#include "logger.hpp"

namespace iris::usb::kbd {

void create(device::Device* dev);
void key(device::Device* dev, uint8_t usage, int pressed);

}
