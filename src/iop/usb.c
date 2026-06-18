#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "usb.h"

// Logging level:
//   0 = off
//   1 = config/enumeration/port/key/report events (low frequency)
//   2 = everything, including per-frame periodic walks and interrupt NAK polls
#define USB_DEBUG 0

#if USB_DEBUG >= 1
#define usb_log(...) printf("usb: " __VA_ARGS__)
#else
#define usb_log(...) do {} while (0)
#endif

#if USB_DEBUG >= 2
#define usb_logv(...) printf("usb: " __VA_ARGS__)
#else
#define usb_logv(...) do {} while (0)
#endif

// HcInterruptStatus/HcInterruptEnable/HcInterruptDisable
#define OHCI_INTR_SO   0x00000001 // Scheduling overrun
#define OHCI_INTR_WDH  0x00000002 // Writeback done head
#define OHCI_INTR_SF   0x00000004 // Start of frame
#define OHCI_INTR_RD   0x00000008 // Resume detected
#define OHCI_INTR_UE   0x00000010 // Unrecoverable error
#define OHCI_INTR_FNO  0x00000020 // Frame number overflow
#define OHCI_INTR_RHSC 0x00000040 // Root hub status change
#define OHCI_INTR_OC   0x40000000 // Ownership change
#define OHCI_INTR_MIE  0x80000000 // Master interrupt enable

// HcControl
#define OHCI_CTL_CBSR  0x00000003 // Control/bulk service ratio
#define OHCI_CTL_PLE   0x00000004 // Periodic list enable
#define OHCI_CTL_IE    0x00000008 // Isochronous enable
#define OHCI_CTL_CLE   0x00000010 // Control list enable
#define OHCI_CTL_BLE   0x00000020 // Bulk list enable
#define OHCI_CTL_HCFS  0x000000c0 // Host controller functional state
#define OHCI_CTL_IR    0x00000100 // Interrupt routing
#define OHCI_CTL_RWC   0x00000200 // Remote wakeup connected
#define OHCI_CTL_RWE   0x00000400 // Remote wakeup enable

#define OHCI_USB_RESET       0x00
#define OHCI_USB_RESUME      0x40
#define OHCI_USB_OPERATIONAL 0x80
#define OHCI_USB_SUSPEND     0xc0

// HcCommandStatus
#define OHCI_STATUS_HCR 0x00000001 // Host controller reset
#define OHCI_STATUS_CLF 0x00000002 // Control list filled
#define OHCI_STATUS_BLF 0x00000004 // Bulk list filled
#define OHCI_STATUS_OCR 0x00000008 // Ownership change request

// HcRhPortStatus
#define OHCI_PORT_CCS  0x00000001 // Current connect status
#define OHCI_PORT_PES  0x00000002 // Port enable status
#define OHCI_PORT_PSS  0x00000004 // Port suspend status
#define OHCI_PORT_POCI 0x00000008 // Port over-current indicator
#define OHCI_PORT_PRS  0x00000010 // Port reset status
#define OHCI_PORT_PPS  0x00000100 // Port power status
#define OHCI_PORT_LSDA 0x00000200 // Low speed device attached
#define OHCI_PORT_CSC  0x00010000 // Connect status change
#define OHCI_PORT_PESC 0x00020000 // Port enable status change
#define OHCI_PORT_PSSC 0x00040000 // Port suspend status change
#define OHCI_PORT_OCIC 0x00080000 // Over-current indicator change
#define OHCI_PORT_PRSC 0x00100000 // Port reset status change

// Endpoint descriptor fields (16 bytes: control, tailP, headP, nextED)
#define OHCI_ED_K 0x00004000 // Skip
#define OHCI_ED_F 0x00008000 // Format (1 = isochronous)
#define OHCI_ED_H 0x00000001 // Halted (headP bit 0)
#define OHCI_ED_C 0x00000002 // Toggle carry (headP bit 1)

// Transfer descriptor control field (general TD)
#define OHCI_TD_DP_SHIFT 19
#define OHCI_TD_DP_MASK  0x3
#define OHCI_TD_CC_SHIFT 28

// OHCI completion (condition) codes
#define OHCI_CC_NOERROR        0x0
#define OHCI_CC_STALL          0x4
#define OHCI_CC_DEVICENOTRESP  0x5

// Internal USB packet identifiers (match the OHCI TD direction encoding)
#define USB_PID_SETUP 0
#define USB_PID_OUT   1
#define USB_PID_IN    2

// Device transfer return codes (>= 0 means that many bytes were transferred)
#define USB_ACK_NAK   -1
#define USB_ACK_STALL -2
#define USB_ACK_NODEV -3

// A USB frame is 1 ms. The scheduler is clocked at the EE clock (294.912 MHz).
#define OHCI_FRAME_CYCLES 294912

// HCCA layout offsets
#define HCCA_FRAMENUMBER 0x80
#define HCCA_DONEHEAD    0x84

#define KBD_REPORT_DESC_LEN 63

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

static inline uint32_t ohci_read_dword(struct ps2_usb* usb, uint32_t addr) {
    return iop_bus_read32(usb->bus, addr);
}

static inline void ohci_write_dword(struct ps2_usb* usb, uint32_t addr, uint32_t data) {
    iop_bus_write32(usb->bus, addr, data);
}

static void ohci_read_buf(struct ps2_usb* usb, uint32_t addr, uint8_t* buf, int len) {
    for (int i = 0; i < len; i++)
        buf[i] = iop_bus_read8(usb->bus, addr + i);
}

static void ohci_write_buf(struct ps2_usb* usb, uint32_t addr, const uint8_t* buf, int len) {
    for (int i = 0; i < len; i++)
        iop_bus_write8(usb->bus, addr + i, buf[i]);
}

static int usb_kbd_get_descriptor(struct usb_device* dev, uint16_t value, uint16_t length) {
    int type = value >> 8;
    int index = value & 0xff;
    const uint8_t* src = NULL;
    int len = 0;

    switch (type) {
        case 0x01: src = kbd_device_desc; len = sizeof(kbd_device_desc); break;
        case 0x02: src = kbd_config_desc; len = sizeof(kbd_config_desc); break;
        case 0x21: src = kbd_hid_desc;    len = sizeof(kbd_hid_desc);    break;
        case 0x22: src = kbd_report_desc; len = sizeof(kbd_report_desc); break;
        case 0x03:
            switch (index) {
                case 0: src = kbd_string0; len = sizeof(kbd_string0); break;
                case 1: src = kbd_string1; len = sizeof(kbd_string1); break;
                case 2: src = kbd_string2; len = sizeof(kbd_string2); break;
                default:
                    usb_log("GET_DESCRIPTOR: unknown string index %d\n", index);
                    return 0;
            }
            break;
        default:
            usb_log("GET_DESCRIPTOR: unknown type %02x index %d\n", type, index);
            return 0;
    }

    usb_log("GET_DESCRIPTOR type=%02x index=%d -> %d bytes (host asked %d)\n",
        type, index, len, length);

    (void)length;

    if (len > (int)sizeof(dev->ctrl_buf))
        len = sizeof(dev->ctrl_buf);

    memcpy(dev->ctrl_buf, src, len);

    return len;
}

static void usb_kbd_complete_status(struct usb_device* dev) {
    if (dev->ctrl_set_address) {
        dev->address = dev->pending_address;
        dev->ctrl_set_address = 0;

        usb_log("device address set to %d\n", dev->address);
    }
}

static int usb_kbd_control(struct usb_device* dev, int pid, uint8_t* buf, int len) {
    if (pid == USB_PID_SETUP) {
        if (len < 8)
            return USB_ACK_STALL;

        uint8_t  bmRequestType = buf[0];
        uint8_t  bRequest      = buf[1];
        uint16_t wValue        = buf[2] | (buf[3] << 8);
        uint16_t wIndex        = buf[4] | (buf[5] << 8);
        uint16_t wLength       = buf[6] | (buf[7] << 8);

        usb_log("SETUP bmRequestType=%02x bRequest=%02x wValue=%04x wIndex=%04x wLength=%d\n",
            bmRequestType, bRequest, wValue, wIndex, wLength);

        dev->ctrl_offset = 0;
        dev->ctrl_len = 0;
        dev->ctrl_dir_in = (bmRequestType & 0x80) != 0;
        dev->ctrl_set_address = 0;

        int type = (bmRequestType >> 5) & 3; // 0 = standard, 1 = class

        if (type == 0) {
            switch (bRequest) {
                case 0x00: // GET_STATUS
                    dev->ctrl_buf[0] = 0;
                    dev->ctrl_buf[1] = 0;
                    dev->ctrl_len = 2;
                    break;
                case 0x01: // CLEAR_FEATURE
                case 0x03: // SET_FEATURE
                    break;
                case 0x05: // SET_ADDRESS
                    dev->pending_address = wValue & 0x7f;
                    dev->ctrl_set_address = 1;
                    break;
                case 0x06: // GET_DESCRIPTOR
                    dev->ctrl_len = usb_kbd_get_descriptor(dev, wValue, wLength);
                    break;
                case 0x08: // GET_CONFIGURATION
                    dev->ctrl_buf[0] = dev->configuration;
                    dev->ctrl_len = 1;
                    break;
                case 0x09: // SET_CONFIGURATION
                    dev->configuration = wValue & 0xff;
                    break;
                case 0x0a: // GET_INTERFACE
                    dev->ctrl_buf[0] = 0;
                    dev->ctrl_len = 1;
                    break;
                case 0x0b: // SET_INTERFACE
                    break;
                default:
                    usb_log("STALL: unsupported standard request %02x\n", bRequest);
                    return USB_ACK_STALL;
            }
        } else if (type == 1) {
            // HID class requests
            switch (bRequest) {
                case 0x01: // GET_REPORT
                    memcpy(dev->ctrl_buf, dev->report, 8);
                    dev->ctrl_len = 8;
                    break;
                case 0x02: // GET_IDLE
                    dev->ctrl_buf[0] = dev->idle;
                    dev->ctrl_len = 1;
                    break;
                case 0x03: // GET_PROTOCOL
                    dev->ctrl_buf[0] = dev->protocol;
                    dev->ctrl_len = 1;
                    break;
                case 0x09: // SET_REPORT (OUT data: LED state)
                    dev->ctrl_len = wLength;
                    break;
                case 0x0a: // SET_IDLE
                    dev->idle = (wValue >> 8) & 0xff;
                    break;
                case 0x0b: // SET_PROTOCOL
                    dev->protocol = wValue & 0xff;
                    break;
                default:
                    usb_log("STALL: unsupported class request %02x\n", bRequest);
                    return USB_ACK_STALL;
            }
        } else {
            usb_log("STALL: unsupported request type %d (bmRequestType=%02x)\n", type, bmRequestType);
            return USB_ACK_STALL;
        }

        // For IN data, never return more than the host asked for
        if (dev->ctrl_dir_in && dev->ctrl_len > wLength)
            dev->ctrl_len = wLength;

        return 8;
    }

    if (pid == USB_PID_IN) {
        if (dev->ctrl_dir_in && dev->ctrl_offset < dev->ctrl_len) {
            int n = dev->ctrl_len - dev->ctrl_offset;
            if (n > len) n = len;
            memcpy(buf, dev->ctrl_buf + dev->ctrl_offset, n);
            dev->ctrl_offset += n;
            return n;
        }

        // Zero-length IN == status stage of an OUT/no-data control transfer
        usb_kbd_complete_status(dev);
        return 0;
    }

    if (pid == USB_PID_OUT) {
        if (!dev->ctrl_dir_in && dev->ctrl_offset < dev->ctrl_len) {
            // Data stage of a control write (e.g. SET_REPORT LED state)
            if (len > 0)
                dev->led_state = buf[0];
            dev->ctrl_offset += len;
            return len;
        }

        // Zero-length OUT == status stage of an IN control transfer
        usb_kbd_complete_status(dev);
        return 0;
    }

    return USB_ACK_STALL;
}

static int usb_kbd_interrupt_in(struct usb_device* dev, uint8_t* buf, int len) {
    // Boot keyboards only report on state change, NAK otherwise
    if (!dev->report_dirty) {
        usb_logv("EP1 IN poll: NAK (no new report)\n");
        return USB_ACK_NAK;
    }

    int n = 8;
    if (n > len) n = len;

    memcpy(buf, dev->report, n);
    dev->report_dirty = 0;

    usb_log("EP1 IN: delivering report %02x %02x %02x %02x %02x %02x %02x %02x (n=%d)\n",
        dev->report[0], dev->report[1], dev->report[2], dev->report[3],
        dev->report[4], dev->report[5], dev->report[6], dev->report[7], n);

    return n;
}

static int usb_device_transfer(struct usb_device* dev, int pid, int ep, uint8_t* buf, int len) {
    if (ep == 0)
        return usb_kbd_control(dev, pid, buf, len);

    if (ep == 1 && pid == USB_PID_IN)
        return usb_kbd_interrupt_in(dev, buf, len);

    return USB_ACK_STALL;
}

static void ohci_update_irq(struct ps2_usb* usb) {
    if ((usb->hc_interrupt_enable & OHCI_INTR_MIE) &&
        (usb->hc_interrupt_status & usb->hc_interrupt_enable & 0x7fffffff)) {
        ps2_iop_intc_irq(usb->intc, IOP_INTC_USB);
    }
}

static void ohci_set_interrupt(struct ps2_usb* usb, uint32_t bit) {
    usb->hc_interrupt_status |= bit;
    ohci_update_irq(usb);
}

// Note: Assert RHSC if any root hub port has an unacknowledged change pending. This is
//       needed because a device may already be connected before the driver enables
//       interrupts/goes operational, and a software reset clears HcInterruptStatus.
static void ohci_update_rhsc(struct ps2_usb* usb) {
    uint32_t change_mask = OHCI_PORT_CSC | OHCI_PORT_PESC | OHCI_PORT_PSSC |
                           OHCI_PORT_OCIC | OHCI_PORT_PRSC;

    for (int i = 0; i < OHCI_NUM_PORTS; i++) {
        if (usb->hc_rh_port_status[i] & change_mask) {
            usb_log("root hub change pending on port %d (status=%08x), asserting RHSC\n",
                i, usb->hc_rh_port_status[i]);
            ohci_set_interrupt(usb, OHCI_INTR_RHSC);
            return;
        }
    }
}

static struct usb_device* ohci_find_device(struct ps2_usb* usb, int addr) {
    for (int i = 0; i < OHCI_NUM_PORTS; i++) {
        struct usb_device* dev = &usb->device[i];

        if (dev->connected && dev->address == addr)
            return dev;
    }

    return NULL;
}

// Process the TD at the head of the given ED. The caller's local `ed` copy is
// updated in place (and written back to RAM) so it can keep looping.
// Returns 1 if a TD was retired (or the ED halted), 0 if the device NAKed.
static int ohci_service_td(struct ps2_usb* usb, uint32_t ed_addr, uint32_t* ed) {
    uint32_t head = ed[2] & 0xfffffff0;
    uint32_t tail = ed[1] & 0xfffffff0;

    if (head == tail)
        return 0;

    uint32_t td_addr = head;
    uint32_t td[4];

    for (int i = 0; i < 4; i++)
        td[i] = ohci_read_dword(usb, td_addr + i * 4);

    int fa  = ed[0] & 0x7f;
    int en  = (ed[0] >> 7) & 0xf;
    int dir = (ed[0] >> 11) & 3;

    int dp = (td[0] >> OHCI_TD_DP_SHIFT) & OHCI_TD_DP_MASK;
    int pid;

    if (dir == 1) pid = USB_PID_OUT;
    else if (dir == 2) pid = USB_PID_IN;
    else pid = dp; // direction taken from TD (0 = SETUP, 1 = OUT, 2 = IN)

    uint32_t cbp = td[1];
    uint32_t be  = td[3];
    uint32_t next_td = td[2] & 0xfffffff0;

    int buf_len = 0;
    if (cbp != 0)
        buf_len = (int)(be - cbp) + 1;
    if (buf_len < 0)
        buf_len = 0;

    uint8_t temp[4096];
    if (buf_len > (int)sizeof(temp))
        buf_len = sizeof(temp);

    struct usb_device* dev = ohci_find_device(usb, fa);

    int result;

    if (!dev) {
        usb_log("TD %08x: no device at address %d (ep=%d pid=%d)\n", td_addr, fa, en, pid);
        result = USB_ACK_NODEV;
    } else if (pid == USB_PID_IN) {
        result = usb_device_transfer(dev, pid, en, temp, buf_len);
    } else {
        if (buf_len)
            ohci_read_buf(usb, cbp, temp, buf_len);

        result = usb_device_transfer(dev, pid, en, temp, buf_len);
    }

    // NAK: leave the TD untouched, the HC retries it on a later frame
    if (result == USB_ACK_NAK)
        return 0;

    int cc = OHCI_CC_NOERROR;
    int actual = 0;

    if (result == USB_ACK_NODEV) {
        cc = OHCI_CC_DEVICENOTRESP;
    } else if (result == USB_ACK_STALL) {
        cc = OHCI_CC_STALL;
    } else {
        actual = result;

        if (pid == USB_PID_IN && actual > 0)
            ohci_write_buf(usb, cbp, temp, actual);
    }

    usb_log("TD %08x ed_fa=%d ep=%d pid=%d len=%d -> cc=%d actual=%d\n",
        td_addr, fa, en, pid, buf_len, cc, actual);

    // Update the transfer descriptor: current buffer pointer + condition code
    if (cc == OHCI_CC_NOERROR) {
        if (actual >= buf_len)
            td[1] = 0;          // whole buffer consumed
        else
            td[1] = cbp + actual;
    }

    td[0] = (td[0] & 0x0fffffff) | ((uint32_t)cc << OHCI_TD_CC_SHIFT);

    // Link the retired TD into the writeback (done) queue. The done queue reuses
    // the TD's NextTD field as its link.
    td[2] = usb->done_queue;
    usb->done_queue = td_addr;

    for (int i = 0; i < 4; i++)
        ohci_write_dword(usb, td_addr + i * 4, td[i]);

    // Advance the endpoint, or halt it on error
    if (cc == OHCI_CC_NOERROR)
        ed[2] = next_td | (ed[2] & OHCI_ED_C);
    else
        ed[2] |= OHCI_ED_H;

    ohci_write_dword(usb, ed_addr + 8, ed[2]);

    return 1;
}

static void ohci_service_ed_list(struct ps2_usb* usb, uint32_t ed_addr) {
    int guard = 0;

    while (ed_addr && guard++ < 64) {
        uint32_t ed[4];

        for (int i = 0; i < 4; i++)
            ed[i] = ohci_read_dword(usb, ed_addr + i * 4);

        uint32_t next_ed = ed[3] & 0xfffffff0;

        // Skip flagged, halted or isochronous endpoints
        if (!(ed[0] & OHCI_ED_K) && !(ed[2] & OHCI_ED_H) && !(ed[0] & OHCI_ED_F)) {
            int td_guard = 0;

            while (td_guard++ < 16) {
                if ((ed[2] & 0xfffffff0) == (ed[1] & 0xfffffff0))
                    break;
                if (ed[2] & OHCI_ED_H)
                    break;
                if (!ohci_service_td(usb, ed_addr, ed))
                    break;
            }
        }

        ed_addr = next_ed;
    }
}

static void ohci_schedule_frame(struct ps2_usb* usb);

static void ohci_frame(void* udata, int overshoot) {
    struct ps2_usb* usb = (struct ps2_usb*)udata;

    (void)overshoot;

    ohci_schedule_frame(usb);

    if ((usb->hc_control & OHCI_CTL_HCFS) != OHCI_USB_OPERATIONAL)
        return;

    usb->hc_fm_number = (usb->hc_fm_number + 1) & 0xffff;
    usb->hc_fm_remaining = usb->hc_fm_interval & 0x3fff;

    if (usb->hc_hcca)
        ohci_write_dword(usb, usb->hc_hcca + HCCA_FRAMENUMBER, usb->hc_fm_number & 0xffff);

    ohci_set_interrupt(usb, OHCI_INTR_SF);

    // Periodic (interrupt) list for this frame
    if (usb->hc_control & OHCI_CTL_PLE) {
        uint32_t intr_ed = 0;

        if (usb->hc_hcca)
            intr_ed = ohci_read_dword(usb, usb->hc_hcca + ((usb->hc_fm_number & 0x1f) * 4));

        if (intr_ed) {
            usb_logv("frame %u: periodic ED list head %08x\n", usb->hc_fm_number, intr_ed);
            ohci_service_ed_list(usb, intr_ed);
        }
    }

    // Control list
    if (usb->hc_control & OHCI_CTL_CLE) {
        ohci_service_ed_list(usb, usb->hc_control_head_ed);
        usb->hc_command_status &= ~OHCI_STATUS_CLF;
    }

    // Bulk list
    if (usb->hc_control & OHCI_CTL_BLE) {
        ohci_service_ed_list(usb, usb->hc_bulk_head_ed);
        usb->hc_command_status &= ~OHCI_STATUS_BLF;
    }

    // Write back the done queue once the driver has acknowledged the previous one
    if (usb->done_queue && !(usb->hc_interrupt_status & OHCI_INTR_WDH)) {
        uint32_t done = usb->done_queue;

        // Low bit signals other unmasked interrupts are pending
        if (usb->hc_interrupt_status & usb->hc_interrupt_enable & 0x7fffffff & ~OHCI_INTR_WDH)
            done |= 1;

        if (usb->hc_hcca)
            ohci_write_dword(usb, usb->hc_hcca + HCCA_DONEHEAD, done);

        usb->hc_done_head = usb->done_queue;
        usb->done_queue = 0;

        usb_log("writeback done head %08x, raising WDH (intr_enable=%08x)\n",
            done & ~0xf, usb->hc_interrupt_enable);

        ohci_set_interrupt(usb, OHCI_INTR_WDH);
    }
}

static void ohci_schedule_frame(struct ps2_usb* usb) {
    struct sched_event event;

    event.callback = ohci_frame;
    event.cycles = OHCI_FRAME_CYCLES;
    event.name = "USB OHCI frame";
    event.udata = usb;

    sched_schedule(usb->sched, event);
}

static void ohci_soft_reset(struct ps2_usb* usb) {
    // Software reset (HcCommandStatus.HCR). Operational registers return to
    // their defaults, but the root hub/device connection state is preserved.
    usb->hc_control = OHCI_USB_SUSPEND;
    usb->hc_command_status = 0;
    usb->hc_interrupt_status = 0;
    usb->hc_interrupt_enable = 0;
    usb->hc_hcca = 0;
    usb->hc_period_current_ed = 0;
    usb->hc_control_head_ed = 0;
    usb->hc_control_current_ed = 0;
    usb->hc_bulk_head_ed = 0;
    usb->hc_bulk_current_ed = 0;
    usb->hc_done_head = 0;
    usb->hc_fm_remaining = 0;
    usb->done_queue = 0;

    usb_log("host controller software reset\n");
}

static void ohci_port_write(struct ps2_usb* usb, int port, uint32_t data) {
    uint32_t* ps = &usb->hc_rh_port_status[port];

    usb_log("write RhPortStatus[%d] = %08x (status was %08x)\n", port, data, *ps);

    if (data & OHCI_PORT_CCS)  // ClearPortEnable
        *ps &= ~OHCI_PORT_PES;

    if ((data & OHCI_PORT_PES) && (*ps & OHCI_PORT_CCS)) // SetPortEnable
        *ps |= OHCI_PORT_PES;

    if ((data & OHCI_PORT_PSS) && (*ps & OHCI_PORT_CCS)) // SetPortSuspend
        *ps |= OHCI_PORT_PSS;

    if (data & OHCI_PORT_POCI) // ClearSuspendStatus
        *ps &= ~OHCI_PORT_PSS;

    if ((data & OHCI_PORT_PRS) && (*ps & OHCI_PORT_CCS)) {
        // SetPortReset: a port reset returns the device to the default address
        usb->device[port].address = 0;
        usb->device[port].configuration = 0;
        usb->device[port].pending_address = 0;
        usb->device[port].ctrl_set_address = 0;

        *ps &= ~OHCI_PORT_PRS;
        *ps |= OHCI_PORT_PES | OHCI_PORT_PRSC;

        ohci_set_interrupt(usb, OHCI_INTR_RHSC);

        usb_log("port %d reset\n", port);
    }

    if (data & OHCI_PORT_PPS)  // SetPortPower
        *ps |= OHCI_PORT_PPS;

    if (data & OHCI_PORT_LSDA) // ClearPortPower
        *ps &= ~OHCI_PORT_PPS;

    // Write-1-to-clear change bits
    *ps &= ~(data & (OHCI_PORT_CSC | OHCI_PORT_PESC | OHCI_PORT_PSSC |
                     OHCI_PORT_OCIC | OHCI_PORT_PRSC));
}

static void ohci_attach(struct ps2_usb* usb, int port) {
    struct usb_device* dev = &usb->device[port];

    memset(dev, 0, sizeof(*dev));
    dev->connected = 1;
    dev->protocol = 1; // HID devices default to report protocol

    // Full-speed device present and powered; flag the connect status change
    usb->hc_rh_port_status[port] = OHCI_PORT_CCS | OHCI_PORT_PPS | OHCI_PORT_CSC;

    ohci_set_interrupt(usb, OHCI_INTR_RHSC);

    usb_log("keyboard attached to root hub port %d (status=%08x)\n",
        port, usb->hc_rh_port_status[port]);
}

struct ps2_usb* ps2_usb_create(void) {
    return malloc(sizeof(struct ps2_usb));
}

void ps2_usb_init(struct ps2_usb* usb, struct ps2_iop_intc* intc, struct iop_bus* bus, struct sched_state* sched) {
    memset(usb, 0, sizeof(struct ps2_usb));

    usb->intc = intc;
    usb->bus = bus;
    usb->sched = sched;

    usb->hc_control = OHCI_USB_RESET;
    usb->hc_fm_interval = 0x2edf;
    usb->hc_ls_threshold = 0x628;

    // NDP = OHCI_NUM_PORTS, NPS (no power switching, ports always on), POTPGT = 1
    usb->hc_rh_descriptor_a = (1u << 24) | (1u << 9) | OHCI_NUM_PORTS;
    usb->hc_rh_descriptor_b = 0;
    usb->hc_rh_status = 0;

    // Attach a keyboard to the first root hub port
    ohci_attach(usb, 0);

    ohci_schedule_frame(usb);
}

void ps2_usb_destroy(struct ps2_usb* usb) {
    free(usb);
}

uint64_t ps2_usb_read32(struct ps2_usb* usb, uint32_t addr) {
    addr &= 0xff;

    switch (addr) {
        case USB_HC_REVISION:         return 0x10;
        case USB_HC_CONTROL:          return usb->hc_control;
        case USB_HC_COMMANDSTATUS:    return usb->hc_command_status;
        case USB_HC_INTERRUPTSTATUS:  return usb->hc_interrupt_status;
        case USB_HC_INTERRUPTENABLE:  return usb->hc_interrupt_enable;
        case USB_HC_INTERRUPTDISABLE: return usb->hc_interrupt_enable;
        case USB_HC_HCCA:             return usb->hc_hcca;
        case USB_HC_PERIODCURRENTED:  return usb->hc_period_current_ed;
        case USB_HC_CONTROLHEADED:    return usb->hc_control_head_ed;
        case USB_HC_CONTROLCURRENTED: return usb->hc_control_current_ed;
        case USB_HC_BULKHEADED:       return usb->hc_bulk_head_ed;
        case USB_HC_BULKCURRENTED:    return usb->hc_bulk_current_ed;
        case USB_HC_DONEHEAD:         return usb->hc_done_head;
        case USB_HC_FMINTERVAL:       return usb->hc_fm_interval;
        case USB_HC_FMREMAINING:      return usb->hc_fm_remaining;
        case USB_HC_FMNUMBER:         return usb->hc_fm_number;
        case USB_HC_PERIODICSTART:    return usb->hc_periodic_start;
        case USB_HC_LSTHRESHOLD:      return usb->hc_ls_threshold;
        case USB_HC_RHDESCRIPTORA:    return usb->hc_rh_descriptor_a;
        case USB_HC_RHDESCRIPTORB:    return usb->hc_rh_descriptor_b;
        case USB_HC_RHSTATUS:         return usb->hc_rh_status;
    }

    if (addr >= USB_HC_RHPORTSTATUS && addr < USB_HC_RHPORTSTATUS + OHCI_NUM_PORTS * 4) {
        int port = (addr - USB_HC_RHPORTSTATUS) >> 2;

        usb_log("read RhPortStatus[%d] = %08x\n", port, usb->hc_rh_port_status[port]);

        return usb->hc_rh_port_status[port];
    }

    usb_log("unhandled read at %02x\n", addr);

    return 0;
}

void ps2_usb_write32(struct ps2_usb* usb, uint32_t addr, uint64_t data) {
    addr &= 0xff;

    uint32_t v = (uint32_t)data;

    switch (addr) {
        case USB_HC_REVISION:
            return;
        case USB_HC_CONTROL: {
            uint32_t old_state = usb->hc_control & OHCI_CTL_HCFS;
            usb->hc_control = v;
            uint32_t new_state = v & OHCI_CTL_HCFS;

            if (new_state != old_state) {
                const char* names[] = { "RESET", "RESUME", "OPERATIONAL", "SUSPEND" };
                usb_log("HcControl=%08x state->%s (PLE=%d CLE=%d BLE=%d IE=%d)\n",
                    v, names[new_state >> 6],
                    !!(v & OHCI_CTL_PLE), !!(v & OHCI_CTL_CLE),
                    !!(v & OHCI_CTL_BLE), !!(v & OHCI_CTL_IE));

                // A device may already be attached when the driver starts the
                // controller; let it know via the root hub status change interrupt.
                if (new_state == OHCI_USB_OPERATIONAL)
                    ohci_update_rhsc(usb);
            }
            return;
        }
        case USB_HC_COMMANDSTATUS:
            if (v & OHCI_STATUS_HCR)
                ohci_soft_reset(usb);

            usb->hc_command_status |= v & (OHCI_STATUS_CLF | OHCI_STATUS_BLF | OHCI_STATUS_OCR);

            if (v & OHCI_STATUS_OCR)
                ohci_set_interrupt(usb, OHCI_INTR_OC);

            usb_logv("HcCommandStatus write %08x\n", v);
            return;
        case USB_HC_INTERRUPTSTATUS:
            usb->hc_interrupt_status &= ~v; // write 1 to clear
            ohci_update_irq(usb);
            usb_logv("clear interrupt status %08x -> %08x\n", v, usb->hc_interrupt_status);
            return;
        case USB_HC_INTERRUPTENABLE:
            usb->hc_interrupt_enable |= v;
            ohci_update_irq(usb);
            usb_log("HcInterruptEnable |= %08x -> %08x\n", v, usb->hc_interrupt_enable);
            return;
        case USB_HC_INTERRUPTDISABLE:
            usb->hc_interrupt_enable &= ~v;
            ohci_update_irq(usb);
            usb_log("HcInterruptDisable %08x -> enable %08x\n", v, usb->hc_interrupt_enable);
            return;
        case USB_HC_HCCA:
            usb->hc_hcca = v & 0xffffff00;
            usb_log("HcHCCA = %08x\n", usb->hc_hcca);
            return;
        case USB_HC_PERIODCURRENTED:
            usb->hc_period_current_ed = v & 0xfffffff0;
            return;
        case USB_HC_CONTROLHEADED:
            usb->hc_control_head_ed = v & 0xfffffff0;
            usb_log("HcControlHeadED = %08x\n", usb->hc_control_head_ed);
            return;
        case USB_HC_CONTROLCURRENTED:
            usb->hc_control_current_ed = v & 0xfffffff0;
            return;
        case USB_HC_BULKHEADED:
            usb->hc_bulk_head_ed = v & 0xfffffff0;
            usb_log("HcBulkHeadED = %08x\n", usb->hc_bulk_head_ed);
            return;
        case USB_HC_BULKCURRENTED:
            usb->hc_bulk_current_ed = v & 0xfffffff0;
            return;
        case USB_HC_DONEHEAD:
            usb->hc_done_head = v;
            return;
        case USB_HC_FMINTERVAL:
            usb->hc_fm_interval = v;
            return;
        case USB_HC_FMREMAINING:
            return;
        case USB_HC_FMNUMBER:
            return;
        case USB_HC_PERIODICSTART:
            usb->hc_periodic_start = v;
            return;
        case USB_HC_LSTHRESHOLD:
            usb->hc_ls_threshold = v;
            return;
        case USB_HC_RHDESCRIPTORA:
            // Keep the number of downstream ports fixed
            usb->hc_rh_descriptor_a = (v & 0xffffff00) | OHCI_NUM_PORTS;
            return;
        case USB_HC_RHDESCRIPTORB:
            usb->hc_rh_descriptor_b = v;
            return;
        case USB_HC_RHSTATUS:
            if (v & 0x00010000) { // SetGlobalPower
                for (int i = 0; i < OHCI_NUM_PORTS; i++)
                    usb->hc_rh_port_status[i] |= OHCI_PORT_PPS;
            }
            if (v & 0x00000001) { // ClearGlobalPower
                for (int i = 0; i < OHCI_NUM_PORTS; i++)
                    usb->hc_rh_port_status[i] &= ~OHCI_PORT_PPS;
            }
            if (v & 0x00008000) // SetRemoteWakeupEnable
                usb->hc_rh_status |= 0x00008000;
            if (v & 0x80000000) // ClearRemoteWakeupEnable
                usb->hc_rh_status &= ~0x00008000;
            return;
    }

    if (addr >= USB_HC_RHPORTSTATUS && addr < USB_HC_RHPORTSTATUS + OHCI_NUM_PORTS * 4) {
        int port = (addr - USB_HC_RHPORTSTATUS) >> 2;

        ohci_port_write(usb, port, v);

        return;
    }

    usb_log("unhandled write %08x at %02x\n", v, addr);
}

void ps2_usb_kbd_key(struct ps2_usb* usb, uint8_t usage, int pressed) {
    struct usb_device* dev = &usb->device[0];

    usb_log("kbd_key usage=%02x pressed=%d (connected=%d address=%d config=%d)\n",
        usage, pressed, dev->connected, dev->address, dev->configuration);

    if (!dev->connected)
        return;

    uint8_t* report = dev->report;

    // Modifier keys (Left Ctrl .. Right GUI) live in the modifier bitmap
    if (usage >= 0xE0 && usage <= 0xE7) {
        uint8_t bit = 1 << (usage - 0xE0);
        uint8_t old = report[0];

        if (pressed)
            report[0] |= bit;
        else
            report[0] &= ~bit;

        if (report[0] != old)
            dev->report_dirty = 1;

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
                dev->report_dirty = 1;
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
            // Compact the array so held keys stay contiguous
            uint8_t keys[6];
            int n = 0;

            for (int i = 2; i < 8; i++)
                if (report[i])
                    keys[n++] = report[i];

            for (int i = 0; i < 6; i++)
                report[i + 2] = (i < n) ? keys[i] : 0;

            dev->report_dirty = 1;
        }
    }
}
