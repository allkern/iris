#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "kbd.h"

#define KBD_DEBUG 0

#if KBD_DEBUG >= 1
#define kbd_log(...) printf("usb-kbd: " __VA_ARGS__)
#else
#define kbd_log(...) do {} while (0)
#endif

#if KBD_DEBUG >= 2
#define kbd_logv(...) printf("usb-kbd: " __VA_ARGS__)
#else
#define kbd_logv(...) do {} while (0)
#endif

#define KBD_REPORT_DESC_LEN 63

struct usb_kbd {
    uint8_t protocol; // 0 = boot, 1 = report
    uint8_t idle;
    uint8_t led_state;

    uint8_t report[8];

    int report_dirty;

    uint8_t ctrl_buf[256];
    int ctrl_len;
    int ctrl_offset;
    int ctrl_dir_in;
    int ctrl_set_address;
};

static const uint8_t kbd_report_desc[KBD_REPORT_DESC_LEN] = {
    0x05, 0x01,       // Usage Page (Generic Desktop)
    0x09, 0x06,       // Usage (Keyboard)
    0xA1, 0x01,       // Collection (Application)
    0x05, 0x07,       //   Usage Page (Keyboard/Keypad)
    0x19, 0xE0,       //   Usage Minimum (224, Left Control)
    0x29, 0xE7,       //   Usage Maximum (231, Right GUI)
    0x15, 0x00,       //   Logical Minimum (0)
    0x25, 0x01,       //   Logical Maximum (1)
    0x75, 0x01,       //   Report Size (1)
    0x95, 0x08,       //   Report Count (8)
    0x81, 0x02,       //   Input (Data, Variable, Absolute)  ; modifier byte
    0x95, 0x01,       //   Report Count (1)
    0x75, 0x08,       //   Report Size (8)
    0x81, 0x01,       //   Input (Constant)                  ; reserved byte
    0x95, 0x05,       //   Report Count (5)
    0x75, 0x01,       //   Report Size (1)
    0x05, 0x08,       //   Usage Page (LEDs)
    0x19, 0x01,       //   Usage Minimum (1)
    0x29, 0x05,       //   Usage Maximum (5)
    0x91, 0x02,       //   Output (Data, Variable, Absolute) ; LED report
    0x95, 0x01,       //   Report Count (1)
    0x75, 0x03,       //   Report Size (3)
    0x91, 0x01,       //   Output (Constant)                 ; LED padding
    0x95, 0x06,       //   Report Count (6)
    0x75, 0x08,       //   Report Size (8)
    0x15, 0x00,       //   Logical Minimum (0)
    0x25, 0x65,       //   Logical Maximum (101)
    0x05, 0x07,       //   Usage Page (Keyboard/Keypad)
    0x19, 0x00,       //   Usage Minimum (0)
    0x29, 0x65,       //   Usage Maximum (101)
    0x81, 0x00,       //   Input (Data, Array)               ; 6 key codes
    0xC0              // End Collection
};

static const uint8_t kbd_device_desc[18] = {
    18, 0x01,         // bLength, bDescriptorType (DEVICE)
    0x10, 0x01,       // bcdUSB 1.10
    0x00,             // bDeviceClass
    0x00,             // bDeviceSubClass
    0x00,             // bDeviceProtocol
    0x08,             // bMaxPacketSize0
    0x6D, 0x04,       // idVendor  (0x046D)
    0x12, 0xC3,       // idProduct (0xC312)
    0x00, 0x01,       // bcdDevice 1.00
    0x01,             // iManufacturer
    0x02,             // iProduct
    0x00,             // iSerialNumber
    0x01              // bNumConfigurations
};

// HID class descriptor (also embedded inside the configuration descriptor)
static const uint8_t kbd_hid_desc[9] = {
    9, 0x21,          // bLength, bDescriptorType (HID)
    0x10, 0x01,       // bcdHID 1.10
    0x00,             // bCountryCode
    0x01,             // bNumDescriptors
    0x22,             // bDescriptorType (Report)
    KBD_REPORT_DESC_LEN & 0xff, (KBD_REPORT_DESC_LEN >> 8) & 0xff
};

static const uint8_t kbd_config_desc[34] = {
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
    0x01,             // bInterfaceProtocol (Keyboard)
    0x00,             // iInterface
    // HID descriptor
    9, 0x21,
    0x10, 0x01,       // bcdHID 1.10
    0x00,             // bCountryCode
    0x01,             // bNumDescriptors
    0x22,             // bDescriptorType (Report)
    KBD_REPORT_DESC_LEN & 0xff, (KBD_REPORT_DESC_LEN >> 8) & 0xff,
    // Endpoint descriptor (interrupt IN, endpoint 1)
    7, 0x05,
    0x81,             // bEndpointAddress (IN, EP1)
    0x03,             // bmAttributes (Interrupt)
    0x08, 0x00,       // wMaxPacketSize
    0x0A              // bInterval (10 ms)
};

static const uint8_t kbd_string0[4] = { 4, 0x03, 0x09, 0x04 }; // LangID English (US)

static const uint8_t kbd_string1[10] = {
    10, 0x03, 'i', 0, 'r', 0, 'i', 0, 's', 0
};

static const uint8_t kbd_string2[28] = {
    28, 0x03,
    'i', 0, 'r', 0, 'i', 0, 's', 0, ' ', 0,
    'K', 0, 'e', 0, 'y', 0, 'b', 0, 'o', 0, 'a', 0, 'r', 0, 'd', 0
};

static int usb_kbd_get_descriptor(struct usb_kbd* kbd, uint16_t value, uint16_t length) {
    int type = value >> 8;
    int index = value & 0xff;
    const uint8_t* src = NULL;
    int len = 0;

    switch (type) {
        case 0x01: src = kbd_device_desc; len = sizeof(kbd_device_desc); break;
        case 0x02: src = kbd_config_desc; len = sizeof(kbd_config_desc); break;
        case 0x21: src = kbd_hid_desc;    len = sizeof(kbd_hid_desc);    break;
        case 0x22: src = kbd_report_desc; len = sizeof(kbd_report_desc); break;
        case 0x03: {
            switch (index) {
                case 0: src = kbd_string0; len = sizeof(kbd_string0); break;
                case 1: src = kbd_string1; len = sizeof(kbd_string1); break;
                case 2: src = kbd_string2; len = sizeof(kbd_string2); break;

                default: {
                    kbd_log("GET_DESCRIPTOR: unknown string index %d\n", index);

                    return 0;
                }
            }
        } break;

        default: {
            kbd_log("GET_DESCRIPTOR: unknown type %02x index %d\n", type, index);

            return 0;
        }
    }

    kbd_log("GET_DESCRIPTOR type=%02x index=%d -> %d bytes (host asked %d)\n",
        type, index, len, length);

    (void)length;

    if (len > (int)sizeof(kbd->ctrl_buf))
        len = sizeof(kbd->ctrl_buf);

    memcpy(kbd->ctrl_buf, src, len);

    return len;
}

static void usb_kbd_complete_status(struct usb_device* dev, struct usb_kbd* kbd) {
    if (kbd->ctrl_set_address) {
        dev->address = dev->pending_address;
        kbd->ctrl_set_address = 0;

        kbd_log("device address set to %d\n", dev->address);
    }
}

static int usb_kbd_control(struct usb_device* dev, int pid, uint8_t* buf, int len) {
    struct usb_kbd* kbd = dev->priv;

    if (pid == USB_PID_SETUP) {
        if (len < 8)
            return USB_ACK_STALL;

        uint8_t  bmRequestType = buf[0];
        uint8_t  bRequest      = buf[1];
        uint16_t wValue        = buf[2] | (buf[3] << 8);
        uint16_t wIndex        = buf[4] | (buf[5] << 8);
        uint16_t wLength       = buf[6] | (buf[7] << 8);

        kbd_log("SETUP bmRequestType=%02x bRequest=%02x wValue=%04x wIndex=%04x wLength=%d\n",
            bmRequestType, bRequest, wValue, wIndex, wLength);

        kbd->ctrl_offset = 0;
        kbd->ctrl_len = 0;
        kbd->ctrl_dir_in = (bmRequestType & 0x80) != 0;
        kbd->ctrl_set_address = 0;

        int type = (bmRequestType >> 5) & 3; // 0 = standard, 1 = class

        if (type == 0) {
            switch (bRequest) {
                case 0x00: { // GET_STATUS
                    kbd->ctrl_buf[0] = 0;
                    kbd->ctrl_buf[1] = 0;
                    kbd->ctrl_len = 2;
                } break;

                case 0x01: // CLEAR_FEATURE
                case 0x03: { // SET_FEATURE
                    break;
                } break;

                case 0x05: { // SET_ADDRESS
                    dev->pending_address = wValue & 0x7f;
                    kbd->ctrl_set_address = 1;
                } break;

                case 0x06: { // GET_DESCRIPTOR
                    kbd->ctrl_len = usb_kbd_get_descriptor(kbd, wValue, wLength);
                } break;

                case 0x08: { // GET_CONFIGURATION
                    kbd->ctrl_buf[0] = dev->configuration;
                    kbd->ctrl_len = 1;
                } break;

                case 0x09: { // SET_CONFIGURATION
                    dev->configuration = wValue & 0xff;
                    break;
                } break;

                case 0x0a: { // GET_INTERFACE
                    kbd->ctrl_buf[0] = 0;
                    kbd->ctrl_len = 1;
                    break;
                } break;

                case 0x0b: { // SET_INTERFACE
                    break;
                } break;

                default: {
                    kbd_log("STALL: unsupported standard request %02x\n", bRequest);

                    return USB_ACK_STALL;
                }
            }
        } else if (type == 1) {
            // HID class requests
            switch (bRequest) {
                case 0x01: { // GET_REPORT
                    memcpy(kbd->ctrl_buf, kbd->report, 8);
                    kbd->ctrl_len = 8;
                } break;

                case 0x02: { // GET_IDLE
                    kbd->ctrl_buf[0] = kbd->idle;
                    kbd->ctrl_len = 1;
                } break;

                case 0x03: { // GET_PROTOCOL
                    kbd->ctrl_buf[0] = kbd->protocol;
                    kbd->ctrl_len = 1;
                } break;

                case 0x09: { // SET_REPORT (OUT data: LED state)
                    kbd->ctrl_len = wLength;
                } break;

                case 0x0a: { // SET_IDLE
                    kbd->idle = (wValue >> 8) & 0xff;
                } break;

                case 0x0b: { // SET_PROTOCOL
                    kbd->protocol = wValue & 0xff;
                } break;

                default: {
                    kbd_log("STALL: unsupported class request %02x\n", bRequest);

                    return USB_ACK_STALL;
                }
            }
        } else {
            kbd_log("STALL: unsupported request type %d (bmRequestType=%02x)\n", type, bmRequestType);

            return USB_ACK_STALL;
        }

        if (kbd->ctrl_dir_in && kbd->ctrl_len > wLength)
            kbd->ctrl_len = wLength;

        return 8;
    }

    if (pid == USB_PID_IN) {
        if (kbd->ctrl_dir_in && kbd->ctrl_offset < kbd->ctrl_len) {
            int n = kbd->ctrl_len - kbd->ctrl_offset;

            if (n > len) {
                n = len;
            }

            memcpy(buf, kbd->ctrl_buf + kbd->ctrl_offset, n);

            kbd->ctrl_offset += n;

            return n;
        }

        usb_kbd_complete_status(dev, kbd);

        return 0;
    }

    if (pid == USB_PID_OUT) {
        if (!kbd->ctrl_dir_in && kbd->ctrl_offset < kbd->ctrl_len) {
            if (len > 0) {
                kbd->led_state = buf[0];
            }

            kbd->ctrl_offset += len;

            return len;
        }

        usb_kbd_complete_status(dev, kbd);

        return 0;
    }

    return USB_ACK_STALL;
}

static int usb_kbd_interrupt_in(struct usb_kbd* kbd, uint8_t* buf, int len) {
    if (!kbd->report_dirty) {
        kbd_logv("EP1 IN poll: NAK (no new report)\n");

        return USB_ACK_NAK;
    }

    int n = 8;

    if (n > len) n = len;

    memcpy(buf, kbd->report, n);

    kbd->report_dirty = 0;

    kbd_log("EP1 IN: delivering report %02x %02x %02x %02x %02x %02x %02x %02x (n=%d)\n",
        kbd->report[0], kbd->report[1], kbd->report[2], kbd->report[3],
        kbd->report[4], kbd->report[5], kbd->report[6], kbd->report[7], n);

    return n;
}

static int usb_kbd_transfer(struct usb_device* dev, int pid, int ep, uint8_t* buf, int len) {
    if (ep == 0)
        return usb_kbd_control(dev, pid, buf, len);

    if (ep == 1 && pid == USB_PID_IN)
        return usb_kbd_interrupt_in(dev->priv, buf, len);

    return USB_ACK_STALL;
}

static void usb_kbd_reset(struct usb_device* dev) {
    struct usb_kbd* kbd = dev->priv;

    kbd->ctrl_set_address = 0;
    kbd->ctrl_offset = 0;
    kbd->ctrl_len = 0;
    kbd->ctrl_dir_in = 0;
}

static void usb_kbd_free(struct usb_device* dev) {
    free(dev->priv);
}

static const struct usb_device_ops usb_kbd_ops = {
    .transfer = usb_kbd_transfer,
    .reset    = usb_kbd_reset,
    .free     = usb_kbd_free,
};

void usb_kbd_create(struct usb_device* dev) {
    struct usb_kbd* kbd = malloc(sizeof(struct usb_kbd));

    memset(kbd, 0, sizeof(struct usb_kbd));

    kbd->protocol = 1; // HID devices default to report protocol

    dev->connected = 1;
    dev->address = 0;
    dev->pending_address = 0;
    dev->configuration = 0;
    dev->ops = &usb_kbd_ops;
    dev->priv = kbd;
}

void usb_kbd_key(struct usb_device* dev, uint8_t usage, int pressed) {
    if (!dev->connected)
        return;

    struct usb_kbd* kbd = dev->priv;

    uint8_t* report = kbd->report;

    kbd_log("kbd_key usage=%02x pressed=%d (connected=%d address=%d config=%d)\n",
        usage, pressed, dev->connected, dev->address, dev->configuration);

    if (usage >= 0xE0 && usage <= 0xE7) {
        uint8_t bit = 1 << (usage - 0xE0);
        uint8_t old = report[0];

        if (pressed)
            report[0] |= bit;
        else
            report[0] &= ~bit;

        if (report[0] != old)
            kbd->report_dirty = 1;

        return;
    }

    if (usage == 0)
        return;

    if (pressed) {
        // Ignore if already held
        for (int i = 2; i < 8; i++)
            if (report[i] == usage)
                return;

        // Insert into the first free rollover slot
        for (int i = 2; i < 8; i++) {
            if (report[i] == 0) {
                report[i] = usage;
                kbd->report_dirty = 1;
                return;
            }
        }
    } else {
        int found = 0;

        for (int i = 2; i < 8; i++) {
            if (report[i] == usage) {
                report[i] = 0;
                found = 1;
            }
        }

        if (found) {
            uint8_t keys[6];

            int n = 0;

            for (int i = 2; i < 8; i++)
                if (report[i])
                    keys[n++] = report[i];

            for (int i = 0; i < 6; i++)
                report[i + 2] = (i < n) ? keys[i] : 0;

            kbd->report_dirty = 1;
        }
    }
}
