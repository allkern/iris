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

namespace iris::speed { struct Speed; }

namespace iris::speed::eeprom {

inline constexpr auto PP_DOUT = 4;
inline constexpr auto PP_DIN = 5;
inline constexpr auto PP_SCLK = 6;
inline constexpr auto PP_CSEL = 7;
inline constexpr auto PP_OP_READ = 2;
inline constexpr auto PP_OP_WRITE = 1;
inline constexpr auto PP_OP_EWEN = 0;
inline constexpr auto PP_OP_EWDS = 0;

enum {
    EEPROM_S_CMD_START,
    EEPROM_S_CMD_READ,
    EEPROM_S_ADDR_READ,
    EEPROM_S_TRANSMIT
};

struct Eeprom {
    int state;

    // Pins
    uint8_t clk;
    uint8_t din;
    uint8_t dout;

    uint8_t cmd;
    uint8_t sequence;
    uint8_t addr;

    uint16_t buf[32];

    logger::Logger* logger = nullptr;
    size_t logger_id = 0;
};

Eeprom* create(logger::Logger* logger);
void init(Eeprom* eeprom);
void load(Eeprom* eeprom, const uint16_t* data);
void destroy(Eeprom* eeprom);
uint64_t read(Eeprom* eeprom);
void write(Eeprom* eeprom, uint64_t data);

}
