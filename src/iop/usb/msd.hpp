#pragma once

#include "device.hpp"
#include "logger.hpp"

namespace iris::usb::msd {

void create(device::Device* dev);
int set_image(device::Device* dev, const char* path);

}
