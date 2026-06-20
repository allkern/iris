#ifndef IOP_USB_MSD_H
#define IOP_USB_MSD_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include "device.h"

void usb_msd_create(struct usb_device* dev);
int usb_msd_set_image(struct usb_device* dev, const char* path);

#ifdef __cplusplus
}
#endif

#endif
