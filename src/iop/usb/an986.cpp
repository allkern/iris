
#include "an986.hpp"

namespace iris::usb::an986 {

inline constexpr auto EP_BULK_IN = 1;
inline constexpr auto EP_BULK_OUT = 2;
inline constexpr auto EP_INTERRUPT_IN = 3;

inline constexpr auto REQUEST_TYPE_STANDARD = 0;
inline constexpr auto REQUEST_TYPE_VENDOR = 2;

inline constexpr auto VENDOR_READ_REGISTER = 0xf0;
inline constexpr auto VENDOR_WRITE_REGISTER = 0xf1;

inline constexpr auto REGISTER_COUNT = 0x40;

inline constexpr auto R_REVISION = 0x01;
inline constexpr auto R_MAC = 0x10;
inline constexpr auto R_MAC_END = 0x16;
inline constexpr auto R_EEPROM_CONTROL = 0x20;
inline constexpr auto R_EEPROM_DATA = 0x21;
inline constexpr auto R_EEPROM_STATUS = 0x23;
inline constexpr auto R_MII_DATA = 0x25;
inline constexpr auto R_MII_CONTROL = 0x28;
inline constexpr auto R_INTERRUPT_STATUS = 0x2b;
inline constexpr auto R_SETUP_DONE = 0x7c;

inline constexpr auto PHY_STATUS_LINK_UP = 0x782d;
inline constexpr auto MII_STATUS_LINK_BITS = 0x24;
inline constexpr auto INTERRUPT_LINK_BITS = 0x60;
inline constexpr auto EEPROM_DONE = 0x04;
inline constexpr auto MII_BUSY_DONE = 0x80;

inline constexpr auto EEPROM_MAC_WORDS = 3;
inline constexpr auto MII_PHY_ADDRESS = 1;

inline constexpr auto FRAME_LENGTH_MIN = 14;
inline constexpr auto FRAME_LENGTH_MAX = 1600;
inline constexpr auto TX_BUFFER_SIZE = 8192;

struct An986 {
    uint8_t registers[REGISTER_COUNT];
    uint8_t mac[6];

    uint8_t eeprom_word;
    uint8_t mii_phy_address;
    uint8_t mii_register;

    int setup_done;
    int link_event_sent;

    uint8_t tx_buf[TX_BUFFER_SIZE];
    int tx_len;

    uint8_t ctrl_buf[256];
    int ctrl_len;
    int ctrl_offset;
    int ctrl_dir_in;
    int ctrl_set_address;

    int pending_register_write;
    int pending_register_index;

    logger::Logger* logger = nullptr;
    size_t logger_id = 0;
};

static const uint8_t an986_device_desc[18] = {
    18, 0x01,
    0x10, 0x01,
    0xff,
    0x00,
    0x00,
    0x40,
    0x9a, 0x0b,
    0x00, 0x05,
    0x01, 0x02,
    0x01,
    0x02,
    0x03,
    0x01
};

static const uint8_t an986_config_desc[39] = {
    9, 0x02,
    39, 0x00,
    0x01,
    0x01,
    0x00,
    0xc0,
    50,

    9, 0x04,
    0x00,
    0x00,
    0x03,
    0xff,
    0x00,
    0x00,
    0x00,

    7, 0x05,
    0x81,
    0x02,
    0x40, 0x00,
    0x00,

    7, 0x05,
    0x02,
    0x02,
    0x40, 0x00,
    0x00,

    7, 0x05,
    0x83,
    0x03,
    0x08, 0x00,
    0x0a
};

static const uint8_t an986_string0[4] = { 4, 0x03, 0x09, 0x04 };

static const uint8_t an986_string1[12] = {
    12, 0x03, 'N', 0, 'a', 0, 'm', 0, 'c', 0, 'o', 0
};

static const uint8_t an986_string2[14] = {
    14, 0x03, 'U', 0, 'E', 0, ' ', 0, 'P', 0, 'C', 0, 'B', 0
};

static const uint8_t an986_string3[4] = { 4, 0x03, '0', 0 };

static const uint8_t default_mac[6] = { 0x00, 0x90, 0x2e, 0x11, 0x22, 0x33 };

static uint16_t mii_read(uint8_t reg) {
    switch (reg) {
        case 0x00: return 0x3100;
        case 0x01: return 0x786d;
        case 0x02: return 0x001d;
        case 0x03: return 0x2411;
        case 0x04: return 0x05e1;
        case 0x05: return 0x0001;
    }

    return 0x0000;
}

static int read_register(An986* an986, int index, uint8_t* out, int length) {
    uint8_t value[8] = {};
    int count = 0;

    if (index == R_INTERRUPT_STATUS && an986->setup_done && !an986->link_event_sent) {
        an986->link_event_sent = 1;

        value[0] = 0x18;
        value[1] = 0x01;
        value[3] = INTERRUPT_LINK_BITS;
        count = 5;
    } else if (index == R_INTERRUPT_STATUS) {
        value[3] = INTERRUPT_LINK_BITS;
        count = 5;
    } else if (index == R_MII_DATA) {
        uint16_t mii = an986->mii_phy_address == MII_PHY_ADDRESS ?
            mii_read(an986->mii_register) : 0xffff;

        value[0] = an986->mii_phy_address;
        value[1] = mii & 0xff;
        value[2] = (mii >> 8) & 0xff;
        value[3] = MII_BUSY_DONE | (an986->mii_register & 0x1f);
        count = 4;
    } else if (index == R_MII_CONTROL) {
        value[0] = MII_BUSY_DONE | (an986->mii_register & 0x1f);
        count = 2;
    } else if (index == R_EEPROM_STATUS) {
        value[0] = EEPROM_DONE;
        count = 2;
    } else if (index == R_EEPROM_DATA) {
        if (an986->eeprom_word < EEPROM_MAC_WORDS) {
            value[0] = an986->mac[an986->eeprom_word * 2];
            value[1] = an986->mac[an986->eeprom_word * 2 + 1];
        }

        value[2] = EEPROM_DONE;
        count = 3;
    } else if (index == R_MAC) {
        memcpy(value, an986->mac, sizeof(an986->mac));
        count = 6;
    } else if (index == R_REVISION) {
        count = 2;
    } else {
        count = length;
    }

    memset(out, 0, length);

    int copied = length;

    if (copied > count) copied = count;
    if (copied > (int)sizeof(value)) copied = sizeof(value);

    if (copied > 0)
        memcpy(out, value, copied);

    if (index == R_EEPROM_CONTROL && length >= 2) {
        out[0] = PHY_STATUS_LINK_UP & 0xff;
        out[1] = (PHY_STATUS_LINK_UP >> 8) & 0xff;
    }

    if (index == R_MII_DATA && length >= 2)
        out[1] |= MII_STATUS_LINK_BITS;

    if (index == R_INTERRUPT_STATUS && length >= 4)
        out[3] |= INTERRUPT_LINK_BITS;

    for (int i = 0; i < length; i++) {
        int reg = index + i;

        if (reg >= R_MAC && reg < R_MAC_END)
            out[i] = an986->mac[reg - R_MAC];
    }

    return length;
}

static void write_register(An986* an986, int index, const uint8_t* data, int length) {
    if (index == R_MII_DATA && length >= 4) {
        an986->mii_phy_address = data[0];
        an986->mii_register = data[3] & 0x1f;
    }

    if (index == R_EEPROM_CONTROL && length >= 1)
        an986->eeprom_word = data[0];

    if (index == R_SETUP_DONE)
        an986->setup_done = 1;

    for (int i = 0; i < length; i++) {
        int reg = index + i;

        if (reg < REGISTER_COUNT)
            an986->registers[reg] = data[i];
    }
}

static int get_descriptor(An986* an986, uint16_t value, uint16_t length) {
    int type = value >> 8;
    int index = value & 0xff;
    const uint8_t* src = NULL;
    int len = 0;

    switch (type) {
        case 0x01: src = an986_device_desc; len = sizeof(an986_device_desc); break;
        case 0x02: src = an986_config_desc; len = sizeof(an986_config_desc); break;
        case 0x03: {
            switch (index) {
                case 0: src = an986_string0; len = sizeof(an986_string0); break;
                case 1: src = an986_string1; len = sizeof(an986_string1); break;
                case 2: src = an986_string2; len = sizeof(an986_string2); break;
                case 3: src = an986_string3; len = sizeof(an986_string3); break;

                default: return 0;
            }
        } break;

        default: return 0;
    }

    (void)length;

    if (len > (int)sizeof(an986->ctrl_buf))
        len = sizeof(an986->ctrl_buf);

    memcpy(an986->ctrl_buf, src, len);

    return len;
}

static void complete_status(device::Device* dev, An986* an986) {
    if (an986->ctrl_set_address) {
        dev->address = dev->pending_address;
        an986->ctrl_set_address = 0;
    }

    if (an986->pending_register_write) {
        write_register(an986, an986->pending_register_index, an986->ctrl_buf, an986->ctrl_offset);

        an986->pending_register_write = 0;
    }
}

static int control_setup(device::Device* dev, An986* an986, uint8_t* buf) {
    uint8_t bmRequestType = buf[0];
    uint8_t bRequest = buf[1];
    uint16_t wValue = buf[2] | (buf[3] << 8);
    uint16_t wIndex = buf[4] | (buf[5] << 8);
    uint16_t wLength = buf[6] | (buf[7] << 8);

    an986->ctrl_offset = 0;
    an986->ctrl_len = 0;
    an986->ctrl_dir_in = (bmRequestType & 0x80) != 0;
    an986->ctrl_set_address = 0;
    an986->pending_register_write = 0;

    int type = (bmRequestType >> 5) & 3;

    if (type == REQUEST_TYPE_STANDARD) {
        switch (bRequest) {
            case 0x00: {
                an986->ctrl_buf[0] = 0;
                an986->ctrl_buf[1] = 0;
                an986->ctrl_len = 2;
            } break;

            case 0x01:
            case 0x03: {
            } break;

            case 0x05: {
                dev->pending_address = wValue & 0x7f;
                an986->ctrl_set_address = 1;
            } break;

            case 0x06: {
                an986->ctrl_len = get_descriptor(an986, wValue, wLength);
            } break;

            case 0x08: {
                an986->ctrl_buf[0] = dev->configuration;
                an986->ctrl_len = 1;
            } break;

            case 0x09: {
                dev->configuration = wValue & 0xff;
            } break;

            case 0x0a: {
                an986->ctrl_buf[0] = 0;
                an986->ctrl_len = 1;
            } break;

            case 0x0b: {
            } break;

            default: {
                iris_debug(an986, "STALL: unsupported standard request {:02x}", bRequest);

                return device::USB_ACK_STALL;
            }
        }
    } else if (type == REQUEST_TYPE_VENDOR) {
        switch (bRequest) {
            case VENDOR_READ_REGISTER: {
                int length = wLength;

                if (length > (int)sizeof(an986->ctrl_buf))
                    length = sizeof(an986->ctrl_buf);

                an986->ctrl_len = read_register(an986, wIndex, an986->ctrl_buf, length);
            } break;

            case VENDOR_WRITE_REGISTER: {
                int length = wLength;

                if (length > (int)sizeof(an986->ctrl_buf))
                    length = sizeof(an986->ctrl_buf);

                an986->ctrl_len = length;
                an986->pending_register_write = 1;
                an986->pending_register_index = wIndex;
            } break;

            default: {
                iris_debug(an986, "STALL: unsupported vendor request {:02x}", bRequest);

                return device::USB_ACK_STALL;
            }
        }
    } else {
        iris_debug(an986, "STALL: unsupported request type {} (bmRequestType={:02x})", type, bmRequestType);

        return device::USB_ACK_STALL;
    }

    if (an986->ctrl_dir_in && an986->ctrl_len > wLength)
        an986->ctrl_len = wLength;

    return 8;
}

static int control(device::Device* dev, int pid, uint8_t* buf, int len) {
    An986* an986 = (An986 *)dev->priv;

    if (pid == device::USB_PID_SETUP) {
        if (len < 8)
            return device::USB_ACK_STALL;

        return control_setup(dev, an986, buf);
    }

    if (pid == device::USB_PID_IN) {
        if (an986->ctrl_dir_in && an986->ctrl_offset < an986->ctrl_len) {
            int n = an986->ctrl_len - an986->ctrl_offset;

            if (n > len) {
                n = len;
            }

            memcpy(buf, an986->ctrl_buf + an986->ctrl_offset, n);

            an986->ctrl_offset += n;

            return n;
        }

        complete_status(dev, an986);

        return 0;
    }

    if (pid == device::USB_PID_OUT) {
        if (!an986->ctrl_dir_in && an986->ctrl_offset < an986->ctrl_len) {
            int n = an986->ctrl_len - an986->ctrl_offset;

            if (n > len) {
                n = len;
            }

            memcpy(an986->ctrl_buf + an986->ctrl_offset, buf, n);

            an986->ctrl_offset += n;

            return len;
        }

        complete_status(dev, an986);

        return 0;
    }

    return device::USB_ACK_STALL;
}

static void drain_tx_frames(An986* an986) {
    while (an986->tx_len >= 2) {
        int frame_len = an986->tx_buf[0] | (an986->tx_buf[1] << 8);
        int total = 2 + frame_len;

        if (frame_len < FRAME_LENGTH_MIN || frame_len > FRAME_LENGTH_MAX) {
            an986->tx_len = 0;

            return;
        }

        if (an986->tx_len < total)
            return;

        memmove(an986->tx_buf, an986->tx_buf + total, an986->tx_len - total);

        an986->tx_len -= total;
    }
}

static int bulk_out(An986* an986, uint8_t* buf, int len) {
    if (len <= 0)
        return 0;

    if (an986->tx_len + len > TX_BUFFER_SIZE)
        an986->tx_len = 0;

    if (len > TX_BUFFER_SIZE)
        return len;

    memcpy(an986->tx_buf + an986->tx_len, buf, len);

    an986->tx_len += len;

    drain_tx_frames(an986);

    return len;
}

static int bulk_in(An986* an986, uint8_t* buf, int len) {
    (void)an986;
    (void)buf;
    (void)len;

    return device::USB_ACK_NAK;
}

static int interrupt_in(An986* an986, uint8_t* buf, int len) {
    (void)an986;

    if (len < 1)
        return 0;

    buf[0] = 0x40;

    return 1;
}

static int transfer(device::Device* dev, int pid, int ep, uint8_t* buf, int len) {
    An986* an986 = (An986 *)dev->priv;

    if (ep == 0)
        return control(dev, pid, buf, len);

    if (ep == EP_BULK_IN && pid == device::USB_PID_IN)
        return bulk_in(an986, buf, len);

    if (ep == EP_BULK_OUT && pid == device::USB_PID_OUT)
        return bulk_out(an986, buf, len);

    if (ep == EP_INTERRUPT_IN && pid == device::USB_PID_IN)
        return interrupt_in(an986, buf, len);

    return device::USB_ACK_STALL;
}

static void reset_device(device::Device* dev) {
    An986* an986 = (An986 *)dev->priv;

    memset(an986->registers, 0, sizeof(an986->registers));
    memcpy(an986->registers + R_MAC, an986->mac, sizeof(an986->mac));

    an986->eeprom_word = 0;
    an986->mii_phy_address = MII_PHY_ADDRESS;
    an986->mii_register = 1;
    an986->setup_done = 0;
    an986->link_event_sent = 0;
    an986->tx_len = 0;

    an986->ctrl_set_address = 0;
    an986->ctrl_offset = 0;
    an986->ctrl_len = 0;
    an986->ctrl_dir_in = 0;
    an986->pending_register_write = 0;
}

static void free(device::Device* dev) {
    delete (An986*)dev->priv;
}

static const device::Ops ops = {
    .transfer = transfer,
    .reset    = reset_device,
    .free     = free,
};

void create(device::Device* dev) {
    An986* an986 = new An986();

    memcpy(an986->mac, default_mac, sizeof(an986->mac));

    dev->connected = 1;
    dev->address = 0;
    dev->pending_address = 0;
    dev->configuration = 0;
    dev->ops = &ops;
    dev->priv = an986;

    reset_device(dev);
}

}
