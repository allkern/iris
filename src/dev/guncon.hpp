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

namespace iris::dev::guncon {

enum Button : uint32_t {
    START  = 0x0008,
    CIRCLE = 0x2000,
    CROSS  = 0x4000
};

enum Axis : int {
    X,
    Y
};

struct Guncon {
    int port = 0;
    uint16_t buttons = 0;
    uint16_t x = 0;
    uint16_t y = 0;
    int config_mode = 0;

    logger::Logger* logger = nullptr;
    size_t logger_id = 0;
};

Guncon* attach(logger::Logger* logger, sio2::Sio2* sio2, int port);
void button_press(Guncon* guncon, uint16_t mask);
void button_release(Guncon* guncon, uint16_t mask);
void analog_change(Guncon* guncon, int axis, uint8_t value);
void reset(void* udata);
void detach(void* udata);

}
