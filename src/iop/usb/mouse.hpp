#pragma once

#include "device.hpp"
#include "logger.hpp"

namespace iris::usb::mouse {

// HID boot-protocol mouse button indices (match the report bitmap)
inline constexpr auto USB_MOUSE_BUTTON_LEFT = 0;
inline constexpr auto USB_MOUSE_BUTTON_RIGHT = 1;
inline constexpr auto USB_MOUSE_BUTTON_MIDDLE = 2;

void create(device::Device* dev);

// Frontend input hooks: accumulate relative motion / set a button state.
void move(device::Device* dev, int dx, int dy, int dz);
void button(device::Device* dev, int button, int pressed);

}
