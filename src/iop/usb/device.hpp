/*
    This file is part of the Iris PlayStation 2 emulator project.
    Hosted at https://github.com/allkern/iris

    MIT License

    Copyright (c) 2026 Allkern/Lycoder (Lisandro Alarcón)

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in all
    copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
    SOFTWARE.
*/

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
    size_t logger_id = 0;
};

int transfer(Device* dev, int pid, int ep, uint8_t* buf, int len);
void reset(Device* dev);
void free(Device* dev);

}
