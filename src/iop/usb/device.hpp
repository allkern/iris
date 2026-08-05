#pragma once
#include "logger.hpp"

namespace iris::usb::device {

// Packet identifiers (match the OHCI TD direction encoding)
inline constexpr auto USB_PID_SETUP = 0;
inline constexpr auto USB_PID_OUT = 1;
inline constexpr auto USB_PID_IN = 2;

// Transfer return codes (>= 0 means that many bytes were transferred)
inline constexpr auto USB_ACK_NAK = -1;
inline constexpr auto USB_ACK_STALL = -2;
inline constexpr auto USB_ACK_NODEV = -3;

struct Device;
struct Ops {
    int  (*transfer)(Device* dev, int pid, int ep, uint8_t* buf, int len);
    void (*reset)(Device* dev);
    void (*free)(Device* dev);
};

struct Device {
    int connected;

    uint8_t address;
    uint8_t pending_address;
    uint8_t configuration;

    const Ops* ops;
    void* priv;

    // Set by the host controller so port devices can log
    logger::Logger* logger = nullptr;
};

int transfer(Device* dev, int pid, int ep, uint8_t* buf, int len);
void reset(Device* dev);
void free(Device* dev);

}
