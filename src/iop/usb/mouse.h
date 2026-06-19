#ifndef IOP_USB_MOUSE_H
#define IOP_USB_MOUSE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include "device.h"

// HID boot-protocol mouse button indices (match the report bitmap)
#define USB_MOUSE_BUTTON_LEFT   0
#define USB_MOUSE_BUTTON_RIGHT  1
#define USB_MOUSE_BUTTON_MIDDLE 2

void usb_mouse_create(struct usb_device* dev);

// Frontend input hooks: accumulate relative motion / set a button state.
void usb_mouse_move(struct usb_device* dev, int dx, int dy, int dz);
void usb_mouse_button(struct usb_device* dev, int button, int pressed);

#ifdef __cplusplus
}
#endif

#endif
