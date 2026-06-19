#ifndef IOP_USB_DEVICE_H
#define IOP_USB_DEVICE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

// Packet identifiers (match the OHCI TD direction encoding)
#define USB_PID_SETUP 0
#define USB_PID_OUT   1
#define USB_PID_IN    2

// Transfer return codes (>= 0 means that many bytes were transferred)
#define USB_ACK_NAK   -1
#define USB_ACK_STALL -2
#define USB_ACK_NODEV -3

struct usb_device;

struct usb_device_ops {
    int  (*transfer)(struct usb_device* dev, int pid, int ep, uint8_t* buf, int len);
    void (*reset)(struct usb_device* dev);
    void (*free)(struct usb_device* dev);
};

struct usb_device {
    int connected;

    uint8_t address;
    uint8_t pending_address;
    uint8_t configuration;

    const struct usb_device_ops* ops;
    void* priv;
};

int usb_device_transfer(struct usb_device* dev, int pid, int ep, uint8_t* buf, int len);
void usb_device_reset(struct usb_device* dev);
void usb_device_free(struct usb_device* dev);

#ifdef __cplusplus
}
#endif

#endif
