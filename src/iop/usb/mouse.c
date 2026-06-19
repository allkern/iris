#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "mouse.h"

#define MOUSE_DEBUG 0

#if MOUSE_DEBUG >= 1
#define mouse_log(...) printf("usb-mouse: " __VA_ARGS__)
#else
#define mouse_log(...) do {} while (0)
#endif

#if MOUSE_DEBUG >= 2
#define mouse_logv(...) printf("usb-mouse: " __VA_ARGS__)
#else
#define mouse_logv(...) do {} while (0)
#endif

#define MOUSE_REPORT_DESC_LEN 52
#define MOUSE_REPORT_LEN 4

struct usb_mouse {
    uint8_t protocol; // 0 = boot, 1 = report
    uint8_t idle;

    uint8_t buttons;  // current button state (absolute)
    int dx, dy, dz;   // accumulated relative motion since the last report
    int dirty;        // a report is pending (motion or button change)

    uint8_t ctrl_buf[256];
    int ctrl_len;
    int ctrl_offset;
    int ctrl_dir_in;
    int ctrl_set_address;
};

static const uint8_t mouse_report_desc[MOUSE_REPORT_DESC_LEN] = {
    0x05, 0x01,       // Usage Page (Generic Desktop)
    0x09, 0x02,       // Usage (Mouse)
    0xA1, 0x01,       // Collection (Application)
    0x09, 0x01,       //   Usage (Pointer)
    0xA1, 0x00,       //   Collection (Physical)
    0x05, 0x09,       //     Usage Page (Buttons)
    0x19, 0x01,       //     Usage Minimum (1)
    0x29, 0x03,       //     Usage Maximum (3)
    0x15, 0x00,       //     Logical Minimum (0)
    0x25, 0x01,       //     Logical Maximum (1)
    0x95, 0x03,       //     Report Count (3)
    0x75, 0x01,       //     Report Size (1)
    0x81, 0x02,       //     Input (Data, Variable, Absolute)  ; 3 buttons
    0x95, 0x01,       //     Report Count (1)
    0x75, 0x05,       //     Report Size (5)
    0x81, 0x01,       //     Input (Constant)                  ; padding
    0x05, 0x01,       //     Usage Page (Generic Desktop)
    0x09, 0x30,       //     Usage (X)
    0x09, 0x31,       //     Usage (Y)
    0x09, 0x38,       //     Usage (Wheel)
    0x15, 0x81,       //     Logical Minimum (-127)
    0x25, 0x7F,       //     Logical Maximum (127)
    0x75, 0x08,       //     Report Size (8)
    0x95, 0x03,       //     Report Count (3)
    0x81, 0x06,       //     Input (Data, Variable, Relative)  ; X, Y, Wheel
    0xC0,             //   End Collection
    0xC0              // End Collection
};

static const uint8_t mouse_device_desc[18] = {
    18, 0x01,         // bLength, bDescriptorType (DEVICE)
    0x10, 0x01,       // bcdUSB 1.10
    0x00,             // bDeviceClass
    0x00,             // bDeviceSubClass
    0x00,             // bDeviceProtocol
    0x08,             // bMaxPacketSize0
    0x6D, 0x04,       // idVendor  (0x046D)
    0x50, 0xC0,       // idProduct (0xC050)
    0x00, 0x01,       // bcdDevice 1.00
    0x01,             // iManufacturer
    0x02,             // iProduct
    0x00,             // iSerialNumber
    0x01              // bNumConfigurations
};

// HID class descriptor (also embedded inside the configuration descriptor)
static const uint8_t mouse_hid_desc[9] = {
    9, 0x21,          // bLength, bDescriptorType (HID)
    0x10, 0x01,       // bcdHID 1.10
    0x00,             // bCountryCode
    0x01,             // bNumDescriptors
    0x22,             // bDescriptorType (Report)
    MOUSE_REPORT_DESC_LEN & 0xff, (MOUSE_REPORT_DESC_LEN >> 8) & 0xff
};

static const uint8_t mouse_config_desc[34] = {
    // Configuration descriptor
    9, 0x02,
    34, 0x00,         // wTotalLength
    0x01,             // bNumInterfaces
    0x01,             // bConfigurationValue
    0x00,             // iConfiguration
    0xA0,             // bmAttributes (bus powered, remote wakeup)
    50,               // bMaxPower (100 mA)
    // Interface descriptor
    9, 0x04,
    0x00,             // bInterfaceNumber
    0x00,             // bAlternateSetting
    0x01,             // bNumEndpoints
    0x03,             // bInterfaceClass (HID)
    0x01,             // bInterfaceSubClass (Boot)
    0x02,             // bInterfaceProtocol (Mouse)
    0x00,             // iInterface
    // HID descriptor
    9, 0x21,
    0x10, 0x01,       // bcdHID 1.10
    0x00,             // bCountryCode
    0x01,             // bNumDescriptors
    0x22,             // bDescriptorType (Report)
    MOUSE_REPORT_DESC_LEN & 0xff, (MOUSE_REPORT_DESC_LEN >> 8) & 0xff,
    // Endpoint descriptor (interrupt IN, endpoint 1)
    7, 0x05,
    0x81,             // bEndpointAddress (IN, EP1)
    0x03,             // bmAttributes (Interrupt)
    MOUSE_REPORT_LEN, 0x00, // wMaxPacketSize
    0x0A              // bInterval (10 ms)
};

static const uint8_t mouse_string0[4] = { 4, 0x03, 0x09, 0x04 }; // LangID English (US)

static const uint8_t mouse_string1[10] = {
    10, 0x03, 'i', 0, 'r', 0, 'i', 0, 's', 0
};

static const uint8_t mouse_string2[22] = {
    22, 0x03,
    'i', 0, 'r', 0, 'i', 0, 's', 0, ' ', 0,
    'M', 0, 'o', 0, 'u', 0, 's', 0, 'e', 0
};

static int mouse_clamp(int v) {
    if (v >  127) return  127;
    if (v < -127) return -127;

    return v;
}

static int usb_mouse_get_descriptor(struct usb_mouse* mouse, uint16_t value, uint16_t length) {
    int type = value >> 8;
    int index = value & 0xff;
    const uint8_t* src = NULL;
    int len = 0;

    switch (type) {
        case 0x01: src = mouse_device_desc; len = sizeof(mouse_device_desc); break;
        case 0x02: src = mouse_config_desc; len = sizeof(mouse_config_desc); break;
        case 0x21: src = mouse_hid_desc;    len = sizeof(mouse_hid_desc);    break;
        case 0x22: src = mouse_report_desc; len = sizeof(mouse_report_desc); break;
        case 0x03: {
            switch (index) {
                case 0: src = mouse_string0; len = sizeof(mouse_string0); break;
                case 1: src = mouse_string1; len = sizeof(mouse_string1); break;
                case 2: src = mouse_string2; len = sizeof(mouse_string2); break;

                default: {
                    mouse_log("GET_DESCRIPTOR: unknown string index %d\n", index);

                    return 0;
                }
            }
        } break;

        default: {
            mouse_log("GET_DESCRIPTOR: unknown type %02x index %d\n", type, index);

            return 0;
        }
    }

    mouse_log("GET_DESCRIPTOR type=%02x index=%d -> %d bytes (host asked %d)\n",
        type, index, len, length);

    (void)length;

    if (len > (int)sizeof(mouse->ctrl_buf))
        len = sizeof(mouse->ctrl_buf);

    memcpy(mouse->ctrl_buf, src, len);

    return len;
}

static void usb_mouse_complete_status(struct usb_device* dev, struct usb_mouse* mouse) {
    if (mouse->ctrl_set_address) {
        dev->address = dev->pending_address;
        mouse->ctrl_set_address = 0;

        mouse_log("device address set to %d\n", dev->address);
    }
}

static int usb_mouse_control(struct usb_device* dev, int pid, uint8_t* buf, int len) {
    struct usb_mouse* mouse = dev->priv;

    if (pid == USB_PID_SETUP) {
        if (len < 8)
            return USB_ACK_STALL;

        uint8_t  bmRequestType = buf[0];
        uint8_t  bRequest      = buf[1];
        uint16_t wValue        = buf[2] | (buf[3] << 8);
        uint16_t wIndex        = buf[4] | (buf[5] << 8);
        uint16_t wLength       = buf[6] | (buf[7] << 8);

        mouse_log("SETUP bmRequestType=%02x bRequest=%02x wValue=%04x wIndex=%04x wLength=%d\n",
            bmRequestType, bRequest, wValue, wIndex, wLength);

        mouse->ctrl_offset = 0;
        mouse->ctrl_len = 0;
        mouse->ctrl_dir_in = (bmRequestType & 0x80) != 0;
        mouse->ctrl_set_address = 0;

        int type = (bmRequestType >> 5) & 3; // 0 = standard, 1 = class

        if (type == 0) {
            switch (bRequest) {
                case 0x00: { // GET_STATUS
                    mouse->ctrl_buf[0] = 0;
                    mouse->ctrl_buf[1] = 0;
                    mouse->ctrl_len = 2;
                } break;

                case 0x01: // CLEAR_FEATURE
                case 0x03: { // SET_FEATURE
                } break;

                case 0x05: { // SET_ADDRESS
                    dev->pending_address = wValue & 0x7f;
                    mouse->ctrl_set_address = 1;
                } break;

                case 0x06: { // GET_DESCRIPTOR
                    mouse->ctrl_len = usb_mouse_get_descriptor(mouse, wValue, wLength);
                } break;

                case 0x08: { // GET_CONFIGURATION
                    mouse->ctrl_buf[0] = dev->configuration;
                    mouse->ctrl_len = 1;
                } break;

                case 0x09: { // SET_CONFIGURATION
                    dev->configuration = wValue & 0xff;
                } break;

                case 0x0a: { // GET_INTERFACE
                    mouse->ctrl_buf[0] = 0;
                    mouse->ctrl_len = 1;
                } break;

                case 0x0b: { // SET_INTERFACE
                } break;

                default: {
                    mouse_log("STALL: unsupported standard request %02x\n", bRequest);

                    return USB_ACK_STALL;
                }
            }
        } else if (type == 1) {
            // HID class requests
            switch (bRequest) {
                case 0x01: { // GET_REPORT
                    mouse->ctrl_buf[0] = mouse->buttons;
                    mouse->ctrl_buf[1] = 0;
                    mouse->ctrl_buf[2] = 0;
                    mouse->ctrl_buf[3] = 0;
                    mouse->ctrl_len = MOUSE_REPORT_LEN;
                } break;

                case 0x02: { // GET_IDLE
                    mouse->ctrl_buf[0] = mouse->idle;
                    mouse->ctrl_len = 1;
                } break;

                case 0x03: { // GET_PROTOCOL
                    mouse->ctrl_buf[0] = mouse->protocol;
                    mouse->ctrl_len = 1;
                } break;

                case 0x09: { // SET_REPORT
                    mouse->ctrl_len = wLength;
                } break;

                case 0x0a: { // SET_IDLE
                    mouse->idle = (wValue >> 8) & 0xff;
                } break;

                case 0x0b: { // SET_PROTOCOL
                    mouse->protocol = wValue & 0xff;
                } break;

                default: {
                    mouse_log("STALL: unsupported class request %02x\n", bRequest);

                    return USB_ACK_STALL;
                }
            }
        } else {
            mouse_log("STALL: unsupported request type %d (bmRequestType=%02x)\n", type, bmRequestType);

            return USB_ACK_STALL;
        }

        if (mouse->ctrl_dir_in && mouse->ctrl_len > wLength)
            mouse->ctrl_len = wLength;

        return 8;
    }

    if (pid == USB_PID_IN) {
        if (mouse->ctrl_dir_in && mouse->ctrl_offset < mouse->ctrl_len) {
            int n = mouse->ctrl_len - mouse->ctrl_offset;

            if (n > len) {
                n = len;
            }

            memcpy(buf, mouse->ctrl_buf + mouse->ctrl_offset, n);

            mouse->ctrl_offset += n;

            return n;
        }

        usb_mouse_complete_status(dev, mouse);

        return 0;
    }

    if (pid == USB_PID_OUT) {
        if (!mouse->ctrl_dir_in && mouse->ctrl_offset < mouse->ctrl_len) {
            mouse->ctrl_offset += len;

            return len;
        }

        usb_mouse_complete_status(dev, mouse);

        return 0;
    }

    return USB_ACK_STALL;
}

static int usb_mouse_interrupt_in(struct usb_mouse* mouse, uint8_t* buf, int len) {
    if (!mouse->dirty) {
        mouse_logv("EP1 IN poll: NAK (no new report)\n");

        return USB_ACK_NAK;
    }

    int x = mouse_clamp(mouse->dx);
    int y = mouse_clamp(mouse->dy);
    int z = mouse_clamp(mouse->dz);

    uint8_t report[MOUSE_REPORT_LEN];

    report[0] = mouse->buttons;
    report[1] = (uint8_t)(int8_t)x;
    report[2] = (uint8_t)(int8_t)y;
    report[3] = (uint8_t)(int8_t)z;

    int moved = x || y || z;

    mouse->dx -= x;
    mouse->dy -= y;
    mouse->dz -= z;

    // Keep reporting while motion remains; once it drains, emit one final
    // zero-motion report. Mouse deltas are relative, so if we simply NAKed the
    // host would keep applying the last non-zero delta and the pointer would
    // never come to rest.
    mouse->dirty = (mouse->dx || mouse->dy || mouse->dz || moved) ? 1 : 0;

    int n = MOUSE_REPORT_LEN;

    if (n > len) n = len;

    memcpy(buf, report, n);

    mouse_log("EP1 IN: delivering report %02x %02x %02x %02x (n=%d)\n",
        report[0], report[1], report[2], report[3], n);

    return n;
}

static int usb_mouse_transfer(struct usb_device* dev, int pid, int ep, uint8_t* buf, int len) {
    if (ep == 0)
        return usb_mouse_control(dev, pid, buf, len);

    if (ep == 1 && pid == USB_PID_IN)
        return usb_mouse_interrupt_in(dev->priv, buf, len);

    return USB_ACK_STALL;
}

static void usb_mouse_reset(struct usb_device* dev) {
    struct usb_mouse* mouse = dev->priv;

    mouse->ctrl_set_address = 0;
    mouse->ctrl_offset = 0;
    mouse->ctrl_len = 0;
    mouse->ctrl_dir_in = 0;
}

static void usb_mouse_free(struct usb_device* dev) {
    free(dev->priv);
}

static const struct usb_device_ops usb_mouse_ops = {
    .transfer = usb_mouse_transfer,
    .reset    = usb_mouse_reset,
    .free     = usb_mouse_free,
};

void usb_mouse_create(struct usb_device* dev) {
    struct usb_mouse* mouse = malloc(sizeof(struct usb_mouse));

    memset(mouse, 0, sizeof(struct usb_mouse));

    mouse->protocol = 1; // HID devices default to report protocol

    dev->connected = 1;
    dev->address = 0;
    dev->pending_address = 0;
    dev->configuration = 0;
    dev->ops = &usb_mouse_ops;
    dev->priv = mouse;
}

void usb_mouse_move(struct usb_device* dev, int dx, int dy, int dz) {
    if (!dev->connected)
        return;

    if (!dx && !dy && !dz)
        return;

    struct usb_mouse* mouse = dev->priv;

    mouse->dx += dx;
    mouse->dy += dy;
    mouse->dz += dz;
    mouse->dirty = 1;
}

void usb_mouse_button(struct usb_device* dev, int button, int pressed) {
    if (!dev->connected)
        return;

    if (button < 0 || button > 2)
        return;

    struct usb_mouse* mouse = dev->priv;

    uint8_t bit = 1 << button;
    uint8_t old = mouse->buttons;

    if (pressed)
        mouse->buttons |= bit;
    else
        mouse->buttons &= ~bit;

    if (mouse->buttons != old)
        mouse->dirty = 1;
}
