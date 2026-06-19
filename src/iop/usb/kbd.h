#ifndef IOP_USB_KBD_H
#define IOP_USB_KBD_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include "device.h"

void usb_kbd_create(struct usb_device* dev);
void usb_kbd_key(struct usb_device* dev, uint8_t usage, int pressed);

#ifdef __cplusplus
}
#endif

#endif
