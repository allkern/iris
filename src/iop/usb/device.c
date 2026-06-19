#include "device.h"

int usb_device_transfer(struct usb_device* dev, int pid, int ep, uint8_t* buf, int len) {
    return dev->ops->transfer(dev, pid, ep, buf, len);
}

void usb_device_reset(struct usb_device* dev) {
    dev->address = 0;
    dev->pending_address = 0;
    dev->configuration = 0;

    if (dev->ops && dev->ops->reset)
        dev->ops->reset(dev);
}

void usb_device_free(struct usb_device* dev) {
    if (dev->ops && dev->ops->free)
        dev->ops->free(dev);

    dev->ops = NULL;
    dev->priv = NULL;
    dev->connected = 0;
}
