#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "msd.h"

#define MSD_DEBUG 0

#if MSD_DEBUG >= 1
#define msd_log(...) printf("usb-msd: " __VA_ARGS__)
#else
#define msd_log(...) do {} while (0)
#endif

#if MSD_DEBUG >= 2
#define msd_logv(...) printf("usb-msd: " __VA_ARGS__)
#else
#define msd_logv(...) do {} while (0)
#endif

#define MSD_BLOCK_SIZE 512

// Bulk-Only Transport wrappers
#define CBW_SIGNATURE 0x43425355 // "USBC"
#define CSW_SIGNATURE 0x53425355 // "USBS"
#define CBW_SIZE 31
#define CSW_SIZE 13

enum {
    MSD_PHASE_CBW,
    MSD_PHASE_DATA_IN,
    MSD_PHASE_DATA_OUT,
    MSD_PHASE_CSW
};

// Where the active data phase reads from / writes to
enum {
    MSD_SRC_NONE,
    MSD_SRC_BUF,  // staged response in data_buf
    MSD_SRC_FILE, // streamed from/to the image
    MSD_SRC_ZERO  // failure padding (IN: zeros, OUT: discard)
};

struct usb_msd {
    // Control transfer state (endpoint 0)
    uint8_t ctrl_buf[256];
    int ctrl_len;
    int ctrl_offset;
    int ctrl_dir_in;
    int ctrl_set_address;

    // Backing store
    FILE* image;
    int write_protect;
    uint32_t block_count; // number of MSD_BLOCK_SIZE blocks

    // Bulk-Only Transport state machine
    int phase;
    uint32_t tag;          // dCBWTag
    uint32_t dcbw_len;     // dCBWDataTransferLength
    uint32_t csw_residue;
    uint8_t  csw_status;   // 0 = pass, 1 = fail

    // Active data phase
    int data_src;
    uint32_t data_remaining;
    uint64_t file_offset;
    uint8_t  data_buf[256];
    uint32_t data_off;

    // REQUEST SENSE state
    uint8_t sense_key, sense_asc, sense_ascq;
};

static const uint8_t msd_device_desc[18] = {
    18, 0x01,         // bLength, bDescriptorType (DEVICE)
    0x00, 0x02,       // bcdUSB 2.00
    0x00,             // bDeviceClass (per interface)
    0x00,             // bDeviceSubClass
    0x00,             // bDeviceProtocol
    0x40,             // bMaxPacketSize0 (64)
    0x81, 0x07,       // idVendor  (0x0781)
    0x67, 0x55,       // idProduct (0x5567)
    0x00, 0x01,       // bcdDevice 1.00
    0x01,             // iManufacturer
    0x02,             // iProduct
    0x03,             // iSerialNumber
    0x01              // bNumConfigurations
};

static const uint8_t msd_config_desc[32] = {
    // Configuration descriptor
    9, 0x02,
    32, 0x00,         // wTotalLength
    0x01,             // bNumInterfaces
    0x01,             // bConfigurationValue
    0x00,             // iConfiguration
    0x80,             // bmAttributes (bus powered)
    50,               // bMaxPower (100 mA)
    // Interface descriptor
    9, 0x04,
    0x00,             // bInterfaceNumber
    0x00,             // bAlternateSetting
    0x02,             // bNumEndpoints
    0x08,             // bInterfaceClass (Mass Storage)
    0x06,             // bInterfaceSubClass (SCSI transparent)
    0x50,             // bInterfaceProtocol (Bulk-Only)
    0x00,             // iInterface
    // Endpoint descriptor (bulk IN, endpoint 1)
    7, 0x05,
    0x81,             // bEndpointAddress (IN, EP1)
    0x02,             // bmAttributes (Bulk)
    0x40, 0x00,       // wMaxPacketSize (64)
    0x00,             // bInterval
    // Endpoint descriptor (bulk OUT, endpoint 2)
    7, 0x05,
    0x02,             // bEndpointAddress (OUT, EP2)
    0x02,             // bmAttributes (Bulk)
    0x40, 0x00,       // wMaxPacketSize (64)
    0x00              // bInterval
};

static const uint8_t msd_string0[4] = { 4, 0x03, 0x09, 0x04 }; // LangID English (US)

static const uint8_t msd_string1[10] = {
    10, 0x03, 'i', 0, 'r', 0, 'i', 0, 's', 0
};

static const uint8_t msd_string2[30] = {
    30, 0x03,
    'i', 0, 'r', 0, 'i', 0, 's', 0, ' ', 0,
    'U', 0, 'S', 0, 'B', 0, ' ', 0,
    'D', 0, 'r', 0, 'i', 0, 'v', 0, 'e', 0
};

// Mass storage requires a serial number of at least 12 characters
static const uint8_t msd_string3[26] = {
    26, 0x03,
    '0', 0, '1', 0, '2', 0, '3', 0, '4', 0, '5', 0,
    '6', 0, '7', 0, '8', 0, '9', 0, 'A', 0, 'B', 0
};

static uint32_t rd32be(const uint8_t* p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}

static uint32_t rd32le(const uint8_t* p) {
    return p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void wr32be(uint8_t* p, uint32_t v) {
    p[0] = v >> 24; p[1] = v >> 16; p[2] = v >> 8; p[3] = v;
}

static void wr32le(uint8_t* p, uint32_t v) {
    p[0] = v; p[1] = v >> 8; p[2] = v >> 16; p[3] = v >> 24;
}

static void msd_set_sense(struct usb_msd* msd, uint8_t key, uint8_t asc, uint8_t ascq) {
    msd->sense_key = key;
    msd->sense_asc = asc;
    msd->sense_ascq = ascq;
}

static int msd_get_descriptor(struct usb_msd* msd, uint16_t value, uint16_t length) {
    int type = value >> 8;
    int index = value & 0xff;
    const uint8_t* src = NULL;
    int len = 0;

    switch (type) {
        case 0x01: src = msd_device_desc; len = sizeof(msd_device_desc); break;
        case 0x02: src = msd_config_desc; len = sizeof(msd_config_desc); break;
        case 0x03: {
            switch (index) {
                case 0: src = msd_string0; len = sizeof(msd_string0); break;
                case 1: src = msd_string1; len = sizeof(msd_string1); break;
                case 2: src = msd_string2; len = sizeof(msd_string2); break;
                case 3: src = msd_string3; len = sizeof(msd_string3); break;

                default: {
                    msd_log("GET_DESCRIPTOR: unknown string index %d\n", index);

                    return 0;
                }
            }
        } break;

        default: {
            msd_log("GET_DESCRIPTOR: unknown type %02x index %d\n", type, index);

            return 0;
        }
    }

    (void)length;

    if (len > (int)sizeof(msd->ctrl_buf))
        len = sizeof(msd->ctrl_buf);

    memcpy(msd->ctrl_buf, src, len);

    return len;
}

static void usb_msd_complete_status(struct usb_device* dev, struct usb_msd* msd) {
    if (msd->ctrl_set_address) {
        dev->address = dev->pending_address;
        msd->ctrl_set_address = 0;

        msd_log("device address set to %d\n", dev->address);
    }
}

static int usb_msd_control(struct usb_device* dev, int pid, uint8_t* buf, int len) {
    struct usb_msd* msd = dev->priv;

    if (pid == USB_PID_SETUP) {
        if (len < 8)
            return USB_ACK_STALL;

        uint8_t  bmRequestType = buf[0];
        uint8_t  bRequest      = buf[1];
        uint16_t wValue        = buf[2] | (buf[3] << 8);
        uint16_t wIndex        = buf[4] | (buf[5] << 8);
        uint16_t wLength       = buf[6] | (buf[7] << 8);

        msd_log("SETUP bmRequestType=%02x bRequest=%02x wValue=%04x wIndex=%04x wLength=%d\n",
            bmRequestType, bRequest, wValue, wIndex, wLength);

        msd->ctrl_offset = 0;
        msd->ctrl_len = 0;
        msd->ctrl_dir_in = (bmRequestType & 0x80) != 0;
        msd->ctrl_set_address = 0;

        int type = (bmRequestType >> 5) & 3; // 0 = standard, 1 = class

        if (type == 0) {
            switch (bRequest) {
                case 0x00: { // GET_STATUS
                    msd->ctrl_buf[0] = 0;
                    msd->ctrl_buf[1] = 0;
                    msd->ctrl_len = 2;
                } break;

                case 0x01: // CLEAR_FEATURE
                case 0x03: { // SET_FEATURE
                } break;

                case 0x05: { // SET_ADDRESS
                    dev->pending_address = wValue & 0x7f;
                    msd->ctrl_set_address = 1;
                } break;

                case 0x06: { // GET_DESCRIPTOR
                    msd->ctrl_len = msd_get_descriptor(msd, wValue, wLength);
                } break;

                case 0x08: { // GET_CONFIGURATION
                    msd->ctrl_buf[0] = dev->configuration;
                    msd->ctrl_len = 1;
                } break;

                case 0x09: { // SET_CONFIGURATION
                    dev->configuration = wValue & 0xff;
                } break;

                case 0x0a: { // GET_INTERFACE
                    msd->ctrl_buf[0] = 0;
                    msd->ctrl_len = 1;
                } break;

                case 0x0b: { // SET_INTERFACE
                } break;

                default: {
                    msd_log("STALL: unsupported standard request %02x\n", bRequest);

                    return USB_ACK_STALL;
                }
            }
        } else if (type == 1) {
            // Mass storage class requests
            switch (bRequest) {
                case 0xfe: { // GET_MAX_LUN
                    msd->ctrl_buf[0] = 0; // single LUN
                    msd->ctrl_len = 1;
                } break;

                case 0xff: { // BULK-ONLY MASS STORAGE RESET
                    msd->phase = MSD_PHASE_CBW;
                } break;

                default: {
                    msd_log("STALL: unsupported class request %02x\n", bRequest);

                    return USB_ACK_STALL;
                }
            }
        } else {
            msd_log("STALL: unsupported request type %d (bmRequestType=%02x)\n", type, bmRequestType);

            return USB_ACK_STALL;
        }

        if (msd->ctrl_dir_in && msd->ctrl_len > wLength)
            msd->ctrl_len = wLength;

        return 8;
    }

    if (pid == USB_PID_IN) {
        if (msd->ctrl_dir_in && msd->ctrl_offset < msd->ctrl_len) {
            int n = msd->ctrl_len - msd->ctrl_offset;

            if (n > len) {
                n = len;
            }

            memcpy(buf, msd->ctrl_buf + msd->ctrl_offset, n);

            msd->ctrl_offset += n;

            return n;
        }

        usb_msd_complete_status(dev, msd);

        return 0;
    }

    if (pid == USB_PID_OUT) {
        if (!msd->ctrl_dir_in && msd->ctrl_offset < msd->ctrl_len) {
            msd->ctrl_offset += len;

            return len;
        }

        usb_msd_complete_status(dev, msd);

        return 0;
    }

    return USB_ACK_STALL;
}

static void msd_scsi(struct usb_msd* msd, const uint8_t* cdb) {
    uint8_t op = cdb[0];

    msd->data_src = MSD_SRC_NONE;
    msd->data_remaining = 0;
    msd->data_off = 0;
    msd->csw_status = 0;

    switch (op) {
        case 0x00: { // TEST UNIT READY
            if (!msd->image) {
                msd->csw_status = 1;
                msd_set_sense(msd, 0x02, 0x3a, 0x00); // not ready, medium not present
            }
        } break;

        case 0x03: { // REQUEST SENSE
            uint8_t* d = msd->data_buf;
            memset(d, 0, 18);
            d[0]  = 0x70; // current error, fixed format
            d[2]  = msd->sense_key;
            d[7]  = 10;   // additional sense length
            d[12] = msd->sense_asc;
            d[13] = msd->sense_ascq;
            msd->data_src = MSD_SRC_BUF;
            msd->data_remaining = msd->dcbw_len < 18 ? msd->dcbw_len : 18;
            msd_set_sense(msd, 0, 0, 0);
        } break;

        case 0x12: { // INQUIRY
            uint8_t* d = msd->data_buf;
            memset(d, 0, 36);
            d[0] = 0x00; // direct access block device
            d[1] = 0x80; // removable
            d[2] = 0x02; // SPC-2
            d[3] = 0x02; // response data format
            d[4] = 31;   // additional length
            memcpy(d + 8,  "iris    ", 8);
            memcpy(d + 16, "USB Drive       ", 16);
            memcpy(d + 32, "1.00", 4);
            msd->data_src = MSD_SRC_BUF;
            msd->data_remaining = msd->dcbw_len < 36 ? msd->dcbw_len : 36;
        } break;

        case 0x1a: { // MODE SENSE(6)
            uint8_t* d = msd->data_buf;
            memset(d, 0, 4);
            d[0] = 3; // mode data length
            d[1] = 0; // medium type
            d[2] = msd->write_protect ? 0x80 : 0x00; // device-specific (WP)
            d[3] = 0; // block descriptor length
            msd->data_src = MSD_SRC_BUF;
            msd->data_remaining = msd->dcbw_len < 4 ? msd->dcbw_len : 4;
        } break;

        case 0x1b: // START STOP UNIT
        case 0x1e: // PREVENT ALLOW MEDIUM REMOVAL
        case 0x35: { // SYNCHRONIZE CACHE
            // Accepted as no-ops
        } break;

        case 0x25: { // READ CAPACITY(10)
            if (!msd->image) {
                msd->csw_status = 1;
                msd_set_sense(msd, 0x02, 0x3a, 0x00);
                break;
            }
            uint8_t* d = msd->data_buf;
            uint32_t last = msd->block_count ? msd->block_count - 1 : 0;
            wr32be(d, last);
            wr32be(d + 4, MSD_BLOCK_SIZE);
            msd->data_src = MSD_SRC_BUF;
            msd->data_remaining = msd->dcbw_len < 8 ? msd->dcbw_len : 8;
        } break;

        case 0x28: { // READ(10)
            uint32_t lba = rd32be(cdb + 2);
            uint32_t blocks = (cdb[7] << 8) | cdb[8];

            if (!msd->image) {
                msd->csw_status = 1;
                msd_set_sense(msd, 0x02, 0x3a, 0x00);
                break;
            }
            if ((uint64_t)lba + blocks > msd->block_count) {
                msd->csw_status = 1;
                msd_set_sense(msd, 0x05, 0x21, 0x00); // LBA out of range
                break;
            }

            uint32_t bytes = blocks * MSD_BLOCK_SIZE;
            msd->file_offset = (uint64_t)lba * MSD_BLOCK_SIZE;
            msd->data_src = MSD_SRC_FILE;
            msd->data_remaining = msd->dcbw_len < bytes ? msd->dcbw_len : bytes;
        } break;

        case 0x2a: { // WRITE(10)
            uint32_t lba = rd32be(cdb + 2);
            uint32_t blocks = (cdb[7] << 8) | cdb[8];

            if (!msd->image) {
                msd->csw_status = 1;
                msd_set_sense(msd, 0x02, 0x3a, 0x00);
                break;
            }
            if (msd->write_protect) {
                msd->csw_status = 1;
                msd_set_sense(msd, 0x07, 0x27, 0x00); // data protect, write protected
                break;
            }
            if ((uint64_t)lba + blocks > msd->block_count) {
                msd->csw_status = 1;
                msd_set_sense(msd, 0x05, 0x21, 0x00);
                break;
            }

            uint32_t bytes = blocks * MSD_BLOCK_SIZE;
            msd->file_offset = (uint64_t)lba * MSD_BLOCK_SIZE;
            msd->data_src = MSD_SRC_FILE;
            msd->data_remaining = msd->dcbw_len < bytes ? msd->dcbw_len : bytes;
        } break;

        default: {
            msd_log("unsupported SCSI op %02x\n", op);
            msd->csw_status = 1;
            msd_set_sense(msd, 0x05, 0x20, 0x00); // illegal request, invalid command
        } break;
    }
}

static int usb_msd_bulk_out(struct usb_device* dev, uint8_t* buf, int len) {
    struct usb_msd* msd = dev->priv;

    if (msd->phase == MSD_PHASE_CBW) {
        if (len < CBW_SIZE || rd32le(buf) != CBW_SIGNATURE) {
            msd_log("invalid CBW (len=%d)\n", len);

            return USB_ACK_STALL;
        }

        msd->tag = rd32le(buf + 4);
        msd->dcbw_len = rd32le(buf + 8);

        int dir_in = (buf[12] & 0x80) != 0;
        const uint8_t* cdb = buf + 15;

        msd_logv("CBW tag=%08x len=%u dir=%s cmd=%02x\n",
            msd->tag, msd->dcbw_len, dir_in ? "in" : "out", cdb[0]);

        msd_scsi(msd, cdb);

        // A failed command still has to satisfy the data phase the host queued
        if (msd->csw_status != 0 && msd->dcbw_len > 0) {
            msd->data_src = MSD_SRC_ZERO;
            msd->data_remaining = msd->dcbw_len;
        }

        msd->csw_residue = msd->dcbw_len - msd->data_remaining;

        if (msd->data_remaining == 0)
            msd->phase = MSD_PHASE_CSW;
        else
            msd->phase = dir_in ? MSD_PHASE_DATA_IN : MSD_PHASE_DATA_OUT;

        return CBW_SIZE;
    }

    if (msd->phase == MSD_PHASE_DATA_OUT) {
        uint32_t n = (uint32_t)len < msd->data_remaining ? (uint32_t)len : msd->data_remaining;

        if (msd->data_src == MSD_SRC_FILE && msd->image) {
            fseek(msd->image, (long)msd->file_offset, SEEK_SET);
            fwrite(buf, 1, n, msd->image);
            msd->file_offset += n;
        }

        msd->data_remaining -= n;

        if (msd->data_remaining == 0) {
            if (msd->data_src == MSD_SRC_FILE && msd->image)
                fflush(msd->image);

            msd->phase = MSD_PHASE_CSW;
        }

        return len;
    }

    return USB_ACK_STALL;
}

static int usb_msd_bulk_in(struct usb_device* dev, uint8_t* buf, int len) {
    struct usb_msd* msd = dev->priv;

    if (msd->phase == MSD_PHASE_DATA_IN) {
        uint32_t n = (uint32_t)len < msd->data_remaining ? (uint32_t)len : msd->data_remaining;

        if (msd->data_src == MSD_SRC_BUF) {
            memcpy(buf, msd->data_buf + msd->data_off, n);
            msd->data_off += n;
        } else if (msd->data_src == MSD_SRC_FILE && msd->image) {
            fseek(msd->image, (long)msd->file_offset, SEEK_SET);
            size_t rd = fread(buf, 1, n, msd->image);
            if (rd < n)
                memset(buf + rd, 0, n - rd);
            msd->file_offset += n;
        } else {
            memset(buf, 0, n);
        }

        msd->data_remaining -= n;

        if (msd->data_remaining == 0)
            msd->phase = MSD_PHASE_CSW;

        return (int)n;
    }

    if (msd->phase == MSD_PHASE_CSW) {
        uint8_t csw[CSW_SIZE];

        wr32le(csw, CSW_SIGNATURE);
        wr32le(csw + 4, msd->tag);
        wr32le(csw + 8, msd->csw_residue);
        csw[12] = msd->csw_status;

        int n = len < CSW_SIZE ? len : CSW_SIZE;
        memcpy(buf, csw, n);

        msd->phase = MSD_PHASE_CBW;

        return n;
    }

    return USB_ACK_STALL;
}

static int usb_msd_transfer(struct usb_device* dev, int pid, int ep, uint8_t* buf, int len) {
    if (ep == 0)
        return usb_msd_control(dev, pid, buf, len);

    if (pid == USB_PID_OUT)
        return usb_msd_bulk_out(dev, buf, len);

    if (pid == USB_PID_IN)
        return usb_msd_bulk_in(dev, buf, len);

    return USB_ACK_STALL;
}

static void usb_msd_reset(struct usb_device* dev) {
    struct usb_msd* msd = dev->priv;

    msd->ctrl_set_address = 0;
    msd->ctrl_offset = 0;
    msd->ctrl_len = 0;
    msd->ctrl_dir_in = 0;
    msd->phase = MSD_PHASE_CBW;

    msd_set_sense(msd, 0, 0, 0);
}

static void usb_msd_free(struct usb_device* dev) {
    struct usb_msd* msd = dev->priv;

    if (msd->image)
        fclose(msd->image);

    free(msd);
}

static const struct usb_device_ops usb_msd_ops = {
    .transfer = usb_msd_transfer,
    .reset    = usb_msd_reset,
    .free     = usb_msd_free,
};

void usb_msd_create(struct usb_device* dev) {
    struct usb_msd* msd = malloc(sizeof(struct usb_msd));

    memset(msd, 0, sizeof(struct usb_msd));

    msd->phase = MSD_PHASE_CBW;

    dev->connected = 1;
    dev->address = 0;
    dev->pending_address = 0;
    dev->configuration = 0;
    dev->ops = &usb_msd_ops;
    dev->priv = msd;
}

int usb_msd_set_image(struct usb_device* dev, const char* path) {
    struct usb_msd* msd = dev->priv;

    if (msd->image) {
        fclose(msd->image);
        msd->image = NULL;
    }

    msd->block_count = 0;
    msd->write_protect = 0;

    if (!path || !path[0])
        return 0; // ejected

    FILE* f = fopen(path, "r+b");

    if (!f) {
        f = fopen(path, "rb");

        if (f)
            msd->write_protect = 1;
    }

    if (!f) {
        msd_log("could not open image '%s'\n", path);

        return 1;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);

    if (size < 0)
        size = 0;

    msd->image = f;
    msd->block_count = (uint32_t)(size / MSD_BLOCK_SIZE);

    msd_log("image '%s' opened: %u blocks%s\n",
        path, msd->block_count, msd->write_protect ? " (read-only)" : "");

    return 0;
}
