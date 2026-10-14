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

#include <cstddef>
#include <cstdint>

#include "iop/sio2.hpp"
#include "logger.hpp"

namespace iris::dev::ds {

enum Button : uint32_t {
    SELECT   = 0x0001,
    L3       = 0x0002,
    R3       = 0x0004,
    START    = 0x0008,
    UP       = 0x0010,
    RIGHT    = 0x0020,
    DOWN     = 0x0040,
    LEFT     = 0x0080,
    L2       = 0x0100,
    R2       = 0x0200,
    L1       = 0x0400,
    R1       = 0x0800,
    TRIANGLE = 0x1000,
    CIRCLE   = 0x2000,
    CROSS    = 0x4000,
    SQUARE   = 0x8000,
    ANALOG   = 0x10000
};

enum Axis : int {
    RIGHT_V,
    RIGHT_H,
    LEFT_V,
    LEFT_H
};

struct Ds {
    int port = 0;
    uint16_t buttons = 0;
    uint8_t ax_right_y = 0;
    uint8_t ax_right_x = 0;
    uint8_t ax_left_y = 0;
    uint8_t ax_left_x = 0;
    int config_mode = 0;
    int act_index = 0;
    int mode_index = 0;
    int mode = 0;
    int vibration[2] = {};
    int mask[2] = {};
    int lock = 0;

    logger::Logger* logger = nullptr;
    size_t logger_id = 0;
};

Ds* attach(logger::Logger* logger, sio2::Sio2* sio2, int port);
void button_press(Ds* ds, uint32_t mask);
void button_release(Ds* ds, uint32_t mask);
void analog_change(Ds* ds, int axis, uint8_t value);
void reset(void* udata);
void detach(void* udata);

}
