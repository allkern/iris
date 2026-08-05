
#include "msd.hpp"

namespace iris::usb::msd {

#ifdef _MSC_VER
#define fseek64 _fseeki64
#define ftell64 _ftelli64
#elif defined(_WIN32)
#define fseek64 fseeko64
#define ftell64 ftello64
#else
#define fseek64 fseek
#define ftell64 ftell
#endif

#define BLOCK_SIZE 512

// Bulk-Only Transport wrappers
#define CBW_SIGNATURE 0x43425355 // "USBC"
#define CSW_SIGNATURE 0x53425355 // "USBS"
#define CBW_SIZE 31
#define CSW_SIZE 13

enum {
    PHASE_CBW,
    PHASE_DATA_IN,
    PHASE_DATA_OUT,
    PHASE_CSW
};

// Where the active data phase reads from / writes to
enum {
    SRC_NONE,
    SRC_BUF,  // staged response in data_buf
    SRC_FILE, // streamed from/to the image
    SRC_ZERO  // failure padding (IN: zeros, OUT: discard)
};

struct Msd {
    // Control transfer state (endpoint 0)
    uint8_t ctrl_buf[256];
    int ctrl_len;
    int ctrl_offset;
    int ctrl_dir_in;
    int ctrl_set_address;

    // Backing store
    FILE* image;
    int write_protect;
    uint32_t block_count; // number of BLOCK_SIZE blocks

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

    logger::Logger* logger = nullptr;
    size_t logger_id = 0;
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

static void msd_set_sense(Msd* msd, uint8_t key, uint8_t asc, uint8_t ascq) {
    msd->sense_key = key;
    msd->sense_asc = asc;
    msd->sense_ascq = ascq;
}

static int msd_get_descriptor(Msd* msd, uint16_t value, uint16_t length) {
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
                    iris_debug(msd, "GET_DESCRIPTOR: unknown string index {}", index);

                    return 0;
                }
            }
        } break;

        default: {
            iris_debug(msd, "GET_DESCRIPTOR: unknown type {:02x} index {}", type, index);

            return 0;
        }
    }

    (void)length;

    if (len > (int)sizeof(msd->ctrl_buf))
        len = sizeof(msd->ctrl_buf);

    memcpy(msd->ctrl_buf, src, len);

    return len;
}

static void complete_status(device::Device* dev, Msd* msd) {
    if (msd->ctrl_set_address) {
        dev->address = dev->pending_address;
        msd->ctrl_set_address = 0;

        // iris_debug(msd, "device address set to {}", dev->address);
    }
}

static int control(device::Device* dev, int pid, uint8_t* buf, int len) {
    Msd* msd = (Msd *)dev->priv;

    if (pid == device::USB_PID_SETUP) {
        if (len < 8)
            return device::USB_ACK_STALL;

        uint8_t  bmRequestType = buf[0];
        uint8_t  bRequest      = buf[1];
        uint16_t wValue        = buf[2] | (buf[3] << 8);
        uint16_t wIndex        = buf[4] | (buf[5] << 8);
        uint16_t wLength       = buf[6] | (buf[7] << 8);

        // iris_debug(msd, "SETUP bmRequestType={:02x} bRequest={:02x} wValue={:04x} wIndex={:04x} wLength={}", //     bmRequestType, bRequest, wValue, wIndex, wLength);

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
                    iris_debug(msd, "STALL: unsupported standard request {:02x}", bRequest);

                    return device::USB_ACK_STALL;
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
                    msd->phase = PHASE_CBW;
                } break;

                default: {
                    iris_debug(msd, "STALL: unsupported class request {:02x}", bRequest);

                    return device::USB_ACK_STALL;
                }
            }
        } else {
            iris_debug(msd, "STALL: unsupported request type {} (bmRequestType={:02x})", type, bmRequestType);

            return device::USB_ACK_STALL;
        }

        if (msd->ctrl_dir_in && msd->ctrl_len > wLength)
            msd->ctrl_len = wLength;

        return 8;
    }

    if (pid == device::USB_PID_IN) {
        if (msd->ctrl_dir_in && msd->ctrl_offset < msd->ctrl_len) {
            int n = msd->ctrl_len - msd->ctrl_offset;

            if (n > len) {
                n = len;
            }

            memcpy(buf, msd->ctrl_buf + msd->ctrl_offset, n);

            msd->ctrl_offset += n;

            return n;
        }

        complete_status(dev, msd);

        return 0;
    }

    if (pid == device::USB_PID_OUT) {
        if (!msd->ctrl_dir_in && msd->ctrl_offset < msd->ctrl_len) {
            msd->ctrl_offset += len;

            return len;
        }

        complete_status(dev, msd);

        return 0;
    }

    return device::USB_ACK_STALL;
}

static void msd_scsi(Msd* msd, const uint8_t* cdb) {
    uint8_t op = cdb[0];

    msd->data_src = SRC_NONE;
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

            msd->data_src = SRC_BUF;
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

            msd->data_src = SRC_BUF;
            msd->data_remaining = msd->dcbw_len < 36 ? msd->dcbw_len : 36;
        } break;

        case 0x1a: { // MODE SENSE(6)
            uint8_t* d = msd->data_buf;

            memset(d, 0, 4);

            d[0] = 3; // mode data length
            d[1] = 0; // medium type
            d[2] = msd->write_protect ? 0x80 : 0x00; // device-specific (WP)
            d[3] = 0; // block descriptor length

            msd->data_src = SRC_BUF;
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
            wr32be(d + 4, BLOCK_SIZE);

            msd->data_src = SRC_BUF;
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

            uint32_t bytes = blocks * BLOCK_SIZE;

            msd->file_offset = (uint64_t)lba * BLOCK_SIZE;
            msd->data_src = SRC_FILE;
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

            uint32_t bytes = blocks * BLOCK_SIZE;
            msd->file_offset = (uint64_t)lba * BLOCK_SIZE;
            msd->data_src = SRC_FILE;
            msd->data_remaining = msd->dcbw_len < bytes ? msd->dcbw_len : bytes;
        } break;

        default: {
            iris_debug(msd, "unsupported SCSI op {:02x}", op);
            msd->csw_status = 1;
            msd_set_sense(msd, 0x05, 0x20, 0x00); // illegal request, invalid command
        } break;
    }
}

static int bulk_out(device::Device* dev, uint8_t* buf, int len) {
    Msd* msd = (Msd *)dev->priv;

    if (msd->phase == PHASE_CBW) {
        if (len < CBW_SIZE || rd32le(buf) != CBW_SIGNATURE) {
            iris_debug(msd, "invalid CBW (len={})", len);

            return device::USB_ACK_STALL;
        }

        msd->tag = rd32le(buf + 4);
        msd->dcbw_len = rd32le(buf + 8);

        int dir_in = (buf[12] & 0x80) != 0;

        const uint8_t* cdb = buf + 15;

        // iris_debug(msd, "CBW tag={:08x} len={} dir={} cmd={:02x}", //     msd->tag, msd->dcbw_len, dir_in ? "in" : "out", cdb[0]);

        msd_scsi(msd, cdb);

        if (msd->csw_status != 0 && msd->dcbw_len > 0) {
            msd->data_src = SRC_ZERO;
            msd->data_remaining = msd->dcbw_len;
        }

        msd->csw_residue = msd->dcbw_len - msd->data_remaining;

        if (msd->data_remaining == 0) {
            msd->phase = PHASE_CSW;
        } else {
            msd->phase = dir_in ? PHASE_DATA_IN : PHASE_DATA_OUT;
        }

        return CBW_SIZE;
    }

    if (msd->phase == PHASE_DATA_OUT) {
        uint32_t n = (uint32_t)len < msd->data_remaining ? (uint32_t)len : msd->data_remaining;

        if (msd->data_src == SRC_FILE && msd->image) {
            fseek64(msd->image, (long)msd->file_offset, SEEK_SET);
            fwrite(buf, 1, n, msd->image);

            msd->file_offset += n;
        }

        msd->data_remaining -= n;

        if (msd->data_remaining == 0) {
            if (msd->data_src == SRC_FILE && msd->image)
                fflush(msd->image);

            msd->phase = PHASE_CSW;
        }

        return len;
    }

    return device::USB_ACK_STALL;
}

static int bulk_in(device::Device* dev, uint8_t* buf, int len) {
    Msd* msd = (Msd *)dev->priv;

    if (msd->phase == PHASE_DATA_IN) {
        uint32_t n = (uint32_t)len < msd->data_remaining ? (uint32_t)len : msd->data_remaining;

        if (msd->data_src == SRC_BUF) {
            memcpy(buf, msd->data_buf + msd->data_off, n);

            msd->data_off += n;
        } else if (msd->data_src == SRC_FILE && msd->image) {
            fseek64(msd->image, (long)msd->file_offset, SEEK_SET);

            size_t rd = fread(buf, 1, n, msd->image);

            if (rd < n)
                memset(buf + rd, 0, n - rd);

            msd->file_offset += n;
        } else {
            memset(buf, 0, n);
        }

        msd->data_remaining -= n;

        if (msd->data_remaining == 0)
            msd->phase = PHASE_CSW;

        return (int)n;
    }

    if (msd->phase == PHASE_CSW) {
        uint8_t csw[CSW_SIZE];

        wr32le(csw, CSW_SIGNATURE);
        wr32le(csw + 4, msd->tag);
        wr32le(csw + 8, msd->csw_residue);

        csw[12] = msd->csw_status;

        int n = len < CSW_SIZE ? len : CSW_SIZE;

        memcpy(buf, csw, n);

        msd->phase = PHASE_CBW;

        return n;
    }

    return device::USB_ACK_STALL;
}

static int transfer(device::Device* dev, int pid, int ep, uint8_t* buf, int len) {
    if (ep == 0)
        return control(dev, pid, buf, len);

    if (pid == device::USB_PID_OUT)
        return bulk_out(dev, buf, len);

    if (pid == device::USB_PID_IN)
        return bulk_in(dev, buf, len);

    return device::USB_ACK_STALL;
}

static void reset(device::Device* dev) {
    Msd* msd = (Msd *)dev->priv;

    msd->ctrl_set_address = 0;
    msd->ctrl_offset = 0;
    msd->ctrl_len = 0;
    msd->ctrl_dir_in = 0;
    msd->phase = PHASE_CBW;

    msd_set_sense(msd, 0, 0, 0);
}

static void free(device::Device* dev) {
    Msd* msd = (Msd *)dev->priv;

    if (msd->image)
        fclose(msd->image);

    delete msd;
}

static const device::Ops ops = {
    .transfer = transfer,
    .reset    = reset,
    .free     = free,
};

void create(device::Device* dev) {
    Msd* msd = new Msd();

    msd->phase = PHASE_CBW;

    dev->connected = 1;
    dev->address = 0;
    dev->pending_address = 0;
    dev->configuration = 0;
    dev->ops = &ops;
    dev->priv = msd;
}

int set_image(device::Device* dev, const char* path) {
    Msd* msd = (Msd *)dev->priv;

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
        iris_debug(msd, "could not open image '{}'", path);

        return 1;
    }

    fseek64(f, 0, SEEK_END);

    long size = ftell64(f);

    if (size < 0)
        size = 0;

    msd->image = f;
    msd->block_count = (uint32_t)(size / BLOCK_SIZE);

    iris_debug(msd, "image '{}' opened: {} blocks{}", path, msd->block_count, msd->write_protect ? " (read-only)" : "");

    return 0;
}

}
