#include <cstdio>
#include <cstring>
#include <string>

#include <fmt/format.h>

#include "p2io.hpp"

namespace iris::kp2::p2io {

using iris::usb::device::Device;

inline constexpr auto EP_BULK_IN = 1;
inline constexpr auto EP_BULK_OUT = 2;
inline constexpr auto EP_INTERRUPT_IN = 3;
inline constexpr auto REQUEST_TYPE_STANDARD = 0;
inline constexpr auto PACKET_MAGIC = 0xaa;
inline constexpr auto PACKET_ESCAPE = 0xff;
inline constexpr auto STATUS_OK = 0x00;
inline constexpr auto HEADER_LENGTH = 2;
inline constexpr auto AV_REPORT_31KHZ = 0x00;
inline constexpr auto AV_REPORT_15KHZ = 0x80;
inline constexpr auto DALLAS_READ_BLACK = 0x00;
inline constexpr auto DALLAS_READ_WHITE = 0x01;
inline constexpr auto INTERRUPT_PAYLOAD_SIZE = 12;
inline constexpr auto SCI_READ_MAX = 200;
inline constexpr auto WATCHDOG_BIT = 0x00000002;

static const uint8_t firmware_version[7] = { 'D', '4', '4', 0x00, 0x01, 0x06, 0x04 };

static const uint8_t device_desc[18] = {
    18, 0x01,
    0x01, 0x01,
    0x00,
    0x00,
    0x00,
    0x08,
    0x00, 0x00,
    0x05, 0x73,
    0x00, 0x01,
    0x00,
    0x00,
    0x00,
    0x01
};

static const uint8_t config_desc[40] = {
    0x09, 0x02, 0x28, 0x00, 0x01, 0x01, 0x00, 0xc0, 0x32,
    0x09, 0x04, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00,
    0x07, 0x05, 0x83, 0x03, 0x10, 0x00, 0x03,
    0x07, 0x05, 0x81, 0x02, 0x40, 0x00, 0x0a,
    0x07, 0x05, 0x02, 0x02, 0x40, 0x00, 0x0a,
    0x34
};

static const uint8_t string0[4] = { 4, 0x03, 0x09, 0x04 };

P2io* from_device(Device* dev) {
    return (P2io*)dev->priv;
}

int load_dongle(P2io* p2io, int which, const char* path) {
    if (which < 0 || which >= DONGLE_COUNT)
        return 0;

    if (!path || !path[0])
        return 0;

    FILE* file = fopen(path, "rb");

    if (!file) {
        iris_error(p2io, "Failed to open dongle \"{}\"", path);

        return 0;
    }

    size_t read = fread(p2io->dongle[which], 1, DONGLE_SIZE, file);

    fclose(file);

    if (read != DONGLE_SIZE) {
        iris_error(p2io, "Dongle \"{}\" is {} bytes, expected {}", path, read, DONGLE_SIZE);

        return 0;
    }

    p2io->dongle_loaded[which] = 1;

    return 1;
}

void set_input_type(P2io* p2io, int type) {
    p2io->input_type = type;

    for (int i = 0; i < PORT_COUNT; i++) {
        p2io->port[i].node_count = 0;

        acio::reset(&p2io->port[i]);
    }

    if (type == INPUT_THRILL_DRIVE) {
        thrilldrive::init_handle(&p2io->thrilldrive_handle);
        thrilldrive::init_belt(&p2io->thrilldrive_belt);

        acio::register_node(&p2io->port[PORT_COM2], thrilldrive::handle_packet, &p2io->thrilldrive_handle, "HNDL");
        acio::register_node(&p2io->port[PORT_COM2], thrilldrive::belt_packet, &p2io->thrilldrive_belt, "BELT");
    }

    iris_info(p2io, "Input type {}, {} node(s) on COM1, {} on COM2",
        type, p2io->port[PORT_COM1].node_count, p2io->port[PORT_COM2].node_count);
}

void set_dip_switches(P2io* p2io, uint8_t value) {
    p2io->dip_switches = value;
}

void set_force_31khz(P2io* p2io, int enabled) {
    p2io->force_31khz = enabled;
}

void press_switch(P2io* p2io, uint32_t mask) {
    p2io->jamma |= mask;
}

void release_switch(P2io* p2io, uint32_t mask) {
    p2io->jamma &= ~mask;
}

void set_analog(P2io* p2io, int channel, uint16_t value) {
    if (channel < 0 || channel >= ANALOG_COUNT)
        return;

    p2io->analog[channel] = value;
}

static float clamp_axis(float value) {
    if (value < 0.0f)
        return 0.0f;

    if (value > 1.0f)
        return 1.0f;

    return value;
}

void set_axis(P2io* p2io, int axis, float value) {
    if (axis < 0 || axis >= AXIS_COUNT)
        return;

    p2io->axis[axis] = clamp_axis(value);

    float steer = p2io->axis[AXIS_STEER_RIGHT] - p2io->axis[AXIS_STEER_LEFT];

    p2io->analog[ANALOG_STEER] = (uint16_t)(ANALOG_CENTER + (steer * (ANALOG_CENTER - 1)));
    p2io->analog[ANALOG_GAS] = (uint16_t)(p2io->axis[AXIS_GAS] * ANALOG_MAX);
    p2io->analog[ANALOG_BRAKE] = (uint16_t)(p2io->axis[AXIS_BRAKE] * ANALOG_MAX);
}

void insert_coin(P2io* p2io, int slot) {
    if (slot < 0 || slot >= 2)
        return;

    p2io->coins[slot]++;
}

static void begin_response(P2io* p2io, uint8_t sequence, uint8_t status) {
    p2io->response_size = 0;

    p2io->response[p2io->response_size++] = PACKET_MAGIC;
    p2io->response[p2io->response_size++] = HEADER_LENGTH;
    p2io->response[p2io->response_size++] = sequence;
    p2io->response[p2io->response_size++] = status;
}

static void push_response(P2io* p2io, uint8_t value) {
    if (p2io->response_size >= RESPONSE_MAX)
        return;

    p2io->response[p2io->response_size++] = value;

    p2io->response[1]++;
}

static void push_response_buffer(P2io* p2io, const uint8_t* data, int size) {
    for (int i = 0; i < size; i++) {
        push_response(p2io, data[i]);
    }
}

static std::string hex(const uint8_t* data, int size) {
    std::string out;

    for (int i = 0; i < size; i++) {
        out += fmt::format("{:02x} ", data[i]);
    }

    return out;
}

static void push_wire(P2io* p2io, uint8_t value) {
    if (p2io->wire_size >= WIRE_MAX)
        return;

    p2io->wire[p2io->wire_size++] = value;
}

static void finish_response(P2io* p2io) {
    p2io->wire_size = 0;
    p2io->wire_read = 0;

    push_wire(p2io, p2io->response[0]);

    for (int i = 1; i < p2io->response_size; i++) {
        uint8_t value = p2io->response[i];

        if (value == PACKET_MAGIC || value == PACKET_ESCAPE) {
            push_wire(p2io, PACKET_ESCAPE);
            push_wire(p2io, value ^ 0xff);

            continue;
        }

        push_wire(p2io, value);
    }

    iris_debug(p2io, "-> {}", hex(p2io->wire, p2io->wire_size));
}

static void handle_dallas(P2io* p2io, const uint8_t* payload, int length) {
    int which = DONGLE_BLACK;

    if (length > 0 && payload[0] == DALLAS_READ_WHITE)
        which = DONGLE_WHITE;

    int loaded = p2io->dongle_loaded[which];

    push_response(p2io, loaded ? 1 : 0);

    if (loaded) {
        push_response_buffer(p2io, p2io->dongle[which], DONGLE_SIZE);
    } else {
        iris_warning(p2io, "Game read {} dongle but none is loaded", which == DONGLE_BLACK ? "black" : "white");

        for (int i = 0; i < DONGLE_SIZE; i++) {
            push_response(p2io, 0);
        }
    }
}

static void handle_sci_read(P2io* p2io, const uint8_t* payload, int length) {
    int index = length > 0 ? payload[0] : 0;
    int wanted = length > 1 ? payload[1] : 0;

    if (index < 0 || index >= PORT_COUNT || wanted <= 0) {
        push_response(p2io, 0);

        return;
    }

    if (wanted > SCI_READ_MAX)
        wanted = SCI_READ_MAX;

    uint8_t buf[SCI_READ_MAX];

    int read = acio::read(&p2io->port[index], buf, wanted);

    push_response(p2io, (uint8_t)read);
    push_response_buffer(p2io, buf, read);
}

static void handle_sci_write(P2io* p2io, const uint8_t* payload, int length) {
    int index = length > 0 ? payload[0] : 0;

    if (index < 0 || index >= PORT_COUNT || length < 2) {
        push_response(p2io, 0);

        return;
    }

    acio::write(&p2io->port[index], payload + 2, length - 2);

    push_response(p2io, (uint8_t)(length - 2));
}

static void handle_command(P2io* p2io, uint8_t cmd, uint8_t sequence, const uint8_t* payload, int length) {
    if (cmd == CMD_RESEND) {
        p2io->wire_read = 0;

        return;
    }

    begin_response(p2io, sequence, STATUS_OK);

    switch (cmd) {
        case CMD_GET_VERSION: {
            push_response_buffer(p2io, firmware_version, sizeof(firmware_version));
        } break;

        case CMD_GET_AV_REPORT: {
            push_response(p2io, p2io->force_31khz ? AV_REPORT_31KHZ : AV_REPORT_15KHZ);
        } break;

        case CMD_READ_DIPSWITCH: {
            push_response(p2io, p2io->dip_switches & 0x7f);
        } break;

        case CMD_DALLAS: {
            handle_dallas(p2io, payload, length);
        } break;

        case CMD_COIN_STOCK: {
            push_response(p2io, 0);
            push_response(p2io, (uint8_t)(p2io->coins[0] >> 8));
            push_response(p2io, (uint8_t)(p2io->coins[0] & 0xff));
            push_response(p2io, (uint8_t)(p2io->coins[1] >> 8));
            push_response(p2io, (uint8_t)(p2io->coins[1] & 0xff));
        } break;

        case CMD_SCI_READ: {
            handle_sci_read(p2io, payload, length);
        } break;

        case CMD_SCI_WRITE: {
            handle_sci_write(p2io, payload, length);
        } break;

        case CMD_SET_WATCHDOG: {
            if (length > 0)
                p2io->watchdog = payload[0];
        } break;

        case CMD_SCI_SETUP: {
            int index = length > 0 ? payload[0] : 0;

            if (index >= 0 && index < PORT_COUNT)
                acio::reset(&p2io->port[index]);

            push_response(p2io, 0);
        } break;

        case CMD_FWRITEMODE:
        case CMD_SET_AV_MASK:
        case CMD_LAMP_OUT:
        case CMD_SEND_IR:
        case CMD_GET_JAMMA_POR:
        case CMD_PORT_READ:
        case CMD_PORT_READ_POR:
        case CMD_JAMMA_START:
        case CMD_COIN_COUNTER:
        case CMD_COIN_BLOCKER:
        case CMD_COIN_COUNTER_OUT: {
        } break;

        default: {
            iris_debug(p2io, "Unhandled command {:02x}", cmd);
        } break;
    }

    finish_response(p2io);
}

static void consume_packet(P2io* p2io, const uint8_t* data, int size) {
    uint8_t frame[256];
    int length = 0;

    for (int i = 1; i < size; i++) {
        uint8_t value = data[i];

        if (value == PACKET_ESCAPE) {
            if (++i >= size)
                return;

            value = data[i] ^ 0xff;
        }

        if (length >= (int)sizeof(frame))
            return;

        frame[length++] = value;
    }

    if (length < 3)
        return;

    uint8_t payload_length = frame[0];
    uint8_t sequence = frame[1];
    uint8_t cmd = frame[2];

    int payload_size = length - 3;

    if (payload_length >= HEADER_LENGTH && payload_size > payload_length - HEADER_LENGTH)
        payload_size = payload_length - HEADER_LENGTH;

    // iris_debug(p2io, "<- cmd {:02x} seq {:02x} {}", cmd, sequence, hex(frame + 3, payload_size));

    handle_command(p2io, cmd, sequence, frame + 3, payload_size);
}

static int bulk_out(P2io* p2io, const uint8_t* buf, int len) {
    int start = -1;

    for (int i = 0; i < len; i++) {
        if (buf[i] == PACKET_MAGIC) {
            start = i;

            break;
        }
    }

    if (start < 0)
        return len;

    consume_packet(p2io, buf + start, len - start);

    return len;
}

static int bulk_in(P2io* p2io, uint8_t* buf, int len) {
    int available = p2io->wire_size - p2io->wire_read;

    if (available <= 0)
        return usb::device::USB_ACK_NAK;

    if (available > len)
        available = len;

    memcpy(buf, p2io->wire + p2io->wire_read, available);

    p2io->wire_read += available;

    return available;
}

static int interrupt_in(P2io* p2io, uint8_t* buf, int len) {
    if (len <= 0)
        return usb::device::USB_ACK_NAK;

    p2io->watchdog_state ^= WATCHDOG_BIT;

    uint32_t state = ~(p2io->jamma | p2io->watchdog_state | 1);

    uint8_t report[INTERRUPT_PAYLOAD_SIZE];

    report[0] = (uint8_t)(state >> 24);
    report[1] = (uint8_t)(state >> 16);
    report[2] = (uint8_t)(state >> 8);
    report[3] = (uint8_t)state;

    for (int i = 0; i < ANALOG_COUNT; i++) {
        report[4 + (i * 2)] = (uint8_t)(p2io->analog[i] >> 8);
        report[5 + (i * 2)] = (uint8_t)(p2io->analog[i] & 0xff);
    }

    int size = len < INTERRUPT_PAYLOAD_SIZE ? len : INTERRUPT_PAYLOAD_SIZE;

    memcpy(buf, report, size);

    return size;
}

static int get_descriptor(P2io* p2io, uint16_t value) {
    int type = value >> 8;
    int index = value & 0xff;

    const uint8_t* src = nullptr;
    int len = 0;

    switch (type) {
        case 0x01: src = device_desc; len = sizeof(device_desc); break;
        case 0x02: src = config_desc; len = sizeof(config_desc); break;
        case 0x03: {
            if (index != 0)
                return 0;

            src = string0;
            len = sizeof(string0);
        } break;

        default: return 0;
    }

    if (len > (int)sizeof(p2io->ctrl_buf))
        len = sizeof(p2io->ctrl_buf);

    memcpy(p2io->ctrl_buf, src, len);

    return len;
}

static void complete_status(Device* dev, P2io* p2io) {
    if (p2io->ctrl_set_address) {
        dev->address = dev->pending_address;

        p2io->ctrl_set_address = 0;
    }
}

static int control_setup(Device* dev, P2io* p2io, uint8_t* buf) {
    uint8_t bmRequestType = buf[0];
    uint8_t bRequest = buf[1];
    uint16_t wValue = buf[2] | (buf[3] << 8);
    uint16_t wLength = buf[6] | (buf[7] << 8);

    p2io->ctrl_offset = 0;
    p2io->ctrl_len = 0;
    p2io->ctrl_dir_in = (bmRequestType & 0x80) != 0;
    p2io->ctrl_set_address = 0;

    int type = (bmRequestType >> 5) & 3;

    if (type != REQUEST_TYPE_STANDARD) {
        iris_debug(p2io, "STALL: unsupported request type {} (bmRequestType={:02x})", type, bmRequestType);

        return usb::device::USB_ACK_STALL;
    }

    switch (bRequest) {
        case 0x00: {
            p2io->ctrl_buf[0] = 0;
            p2io->ctrl_buf[1] = 0;
            p2io->ctrl_len = 2;
        } break;

        case 0x01:
        case 0x03:
        case 0x0b: {
        } break;

        case 0x05: {
            dev->pending_address = wValue & 0x7f;

            p2io->ctrl_set_address = 1;
        } break;

        case 0x06: {
            p2io->ctrl_len = get_descriptor(p2io, wValue);
        } break;

        case 0x08: {
            p2io->ctrl_buf[0] = dev->configuration;
            p2io->ctrl_len = 1;
        } break;

        case 0x09: {
            dev->configuration = wValue & 0xff;
        } break;

        case 0x0a: {
            p2io->ctrl_buf[0] = 0;
            p2io->ctrl_len = 1;
        } break;

        default: {
            iris_debug(p2io, "STALL: unsupported standard request {:02x}", bRequest);

            return usb::device::USB_ACK_STALL;
        }
    }

    if (p2io->ctrl_dir_in && p2io->ctrl_len > wLength)
        p2io->ctrl_len = wLength;

    return 8;
}

static int control(Device* dev, int pid, uint8_t* buf, int len) {
    P2io* p2io = from_device(dev);

    if (pid == usb::device::USB_PID_SETUP) {
        if (len < 8)
            return usb::device::USB_ACK_STALL;

        return control_setup(dev, p2io, buf);
    }

    if (pid == usb::device::USB_PID_IN) {
        if (p2io->ctrl_dir_in && p2io->ctrl_offset < p2io->ctrl_len) {
            int n = p2io->ctrl_len - p2io->ctrl_offset;

            if (n > len)
                n = len;

            memcpy(buf, p2io->ctrl_buf + p2io->ctrl_offset, n);

            p2io->ctrl_offset += n;

            return n;
        }

        complete_status(dev, p2io);

        return 0;
    }

    if (pid == usb::device::USB_PID_OUT) {
        if (!p2io->ctrl_dir_in && p2io->ctrl_offset < p2io->ctrl_len) {
            int n = p2io->ctrl_len - p2io->ctrl_offset;

            if (n > len)
                n = len;

            memcpy(p2io->ctrl_buf + p2io->ctrl_offset, buf, n);

            p2io->ctrl_offset += n;

            return len;
        }

        complete_status(dev, p2io);

        return 0;
    }

    return usb::device::USB_ACK_STALL;
}

static int transfer(Device* dev, int pid, int ep, uint8_t* buf, int len) {
    P2io* p2io = from_device(dev);

    if (ep == 0)
        return control(dev, pid, buf, len);

    if (ep == EP_BULK_IN && pid == usb::device::USB_PID_IN)
        return bulk_in(p2io, buf, len);

    if (ep == EP_BULK_OUT && pid == usb::device::USB_PID_OUT)
        return bulk_out(p2io, buf, len);

    if (ep == EP_INTERRUPT_IN && pid == usb::device::USB_PID_IN)
        return interrupt_in(p2io, buf, len);

    iris_debug(p2io, "STALL: pid {} on endpoint {} ({} bytes)", pid, ep, len);

    return usb::device::USB_ACK_STALL;
}

static void reset_device(Device* dev) {
    P2io* p2io = from_device(dev);

    p2io->jamma = 0;

    memset(p2io->analog, 0, sizeof(p2io->analog));
    memset(p2io->axis, 0, sizeof(p2io->axis));
    memset(p2io->coins, 0, sizeof(p2io->coins));

    p2io->analog[ANALOG_STEER] = ANALOG_CENTER;

    p2io->watchdog = 0;
    p2io->watchdog_state = 0;

    p2io->response_size = 0;
    p2io->wire_size = 0;
    p2io->wire_read = 0;

    p2io->ctrl_len = 0;
    p2io->ctrl_offset = 0;
    p2io->ctrl_dir_in = 0;
    p2io->ctrl_set_address = 0;

    for (int i = 0; i < PORT_COUNT; i++) {
        acio::reset(&p2io->port[i]);
    }
}

static void free_device(Device* dev) {
    delete from_device(dev);
}

static const usb::device::Ops ops = {
    .transfer = transfer,
    .reset = reset_device,
    .free = free_device,
};

void create(Device* dev) {
    P2io* p2io = new P2io();

    p2io->logger = dev->logger;
    p2io->logger_id = logger::register_source(dev->logger, "p2io");

    for (int i = 0; i < PORT_COUNT; i++) {
        acio::init(&p2io->port[i], p2io->logger, p2io->logger_id);
    }

    dev->connected = 1;
    dev->address = 0;
    dev->pending_address = 0;
    dev->configuration = 0;
    dev->ops = &ops;
    dev->priv = p2io;

    reset_device(dev);
}

}
