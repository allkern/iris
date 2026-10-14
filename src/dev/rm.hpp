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

#include <atomic>
#include <cstddef>
#include <cstdint>

#include "iop/sio2.hpp"
#include "logger.hpp"

namespace iris::dev::rm {

enum Code : uint32_t {
    DVD_ONE           = 0x0000D049,
    DVD_TWO           = 0x0010D049,
    DVD_THREE         = 0x0020D049,
    DVD_FOUR          = 0x0030D049,
    DVD_FIVE          = 0x0040D049,
    DVD_SIX           = 0x0050D049,
    DVD_SEVEN         = 0x0060D049,
    DVD_EIGHT         = 0x0070D049,
    DVD_NINE          = 0x0080D049,
    DVD_ZERO          = 0x0090D049,
    DVD_ENTER         = 0x00B0D049,
    DVD_RETURN        = 0x00E0D049,
    DVD_CLEAR         = 0x00F0D049,
    DVD_TITLE         = 0x00A0D149,
    DVD_MENU          = 0x00B0D149,
    DVD_PROGRAM       = 0x00F0D149,
    DVD_TIME          = 0x0080D249,
    DVD_ATOB          = 0x00A0D249,
    DVD_REPEAT        = 0x00C0D249,
    DVD_PREV          = 0x0000D349,
    DVD_NEXT          = 0x0010D349,
    DVD_PLAY          = 0x0020D349,
    DVD_SCAN_BACK     = 0x0030D349,
    DVD_SCAN_FORW     = 0x0040D349,
    DVD_SHUFFLE       = 0x0050D349,
    DVD_STOP          = 0x0080D349,
    DVD_PAUSE         = 0x0090D349,
    DVD_DISPLAY       = 0x0040D549,
    DVD_SLOW_BACK     = 0x0000D649,
    DVD_SLOW_FORW     = 0x0010D649,
    DVD_SUBTITLE      = 0x0030D649,
    DVD_AUDIO         = 0x0040D649,
    DVD_ANGLE         = 0x0050D649,
    DVD_UP            = 0x0090D749,
    DVD_DOWN          = 0x00A0D749,
    DVD_LEFT          = 0x00B0D749,
    DVD_RIGHT         = 0x00C0D749,
    PS2_POWER         = 0x0050D1DA,
    PS2_EJECT         = 0x0060D1DA,
    PS2_RESET         = 0x0010D7DA,
    PS2_SELECT        = 0x0000D5DA,
    PS2_L3            = 0x0010D5DA,
    PS2_R3            = 0x0020D5DA,
    PS2_START         = 0x0030D5DA,
    PS2_UP            = 0x0040D5DA,
    PS2_RIGHT         = 0x0050D5DA,
    PS2_DOWN          = 0x0060D5DA,
    PS2_LEFT          = 0x0070D5DA,
    PS2_L2            = 0x0080D5DA,
    PS2_R2            = 0x0090D5DA,
    PS2_L1            = 0x00A0D5DA,
    PS2_R1            = 0x00B0D5DA,
    PS2_TRIANGLE      = 0x00C0D5DA,
    PS2_CIRCLE        = 0x00D0D5DA,
    PS2_CROSS         = 0x00E0D5DA,
    PS2_SQUARE        = 0x00F0D5DA,
    DESR_EJECT        = 0x0060D193,
    DESR_POWER        = 0x0050D193,
    DESR_ONE          = 0x0000D093,
    DESR_TWO          = 0x0010D093,
    DESR_THREE        = 0x0020D093,
    DESR_FOUR         = 0x0030D093,
    DESR_FIVE         = 0x0040D093,
    DESR_SIX          = 0x0050D093,
    DESR_SEVEN        = 0x0060D093,
    DESR_EIGHT        = 0x0070D093,
    DESR_NINE         = 0x0080D093,
    DESR_TEN          = 0x0090D093,
    DESR_CLEAR        = 0x00F0D093,
    DESR_TOP_MENU     = 0x00C0D293,
    DESR_MENU         = 0x0090D293,
    DESR_RETURN       = 0x0030D493,
    DESR_PROGRAM      = 0x00A0D293,
    DESR_HOME         = 0x0020D493,
    DESR_DISPLAY      = 0x0050D293,
    DESR_G_GUIDE      = 0x0050D493,
    DESR_QUIT_GAME    = 0x0010D693,
    DESR_ENTER        = 0x0000D693,
    DESR_UP           = 0x0040D593,
    DESR_RIGHT        = 0x0050D593,
    DESR_DOWN         = 0x0060D593,
    DESR_LEFT         = 0x0070D593,
    DESR_SELECT       = 0x0000D593,
    DESR_L3           = 0x0010D593,
    DESR_R3           = 0x0020D593,
    DESR_START        = 0x0030D593,
    DESR_L2_SCAN_BACK = 0x0080D593,
    DESR_R2_SCAN_FORW = 0x0090D593,
    DESR_L1_PREV      = 0x00A0D593,
    DESR_R1_NEXT      = 0x00B0D593,
    DESR_TRIANGLE     = 0x00C0D593,
    DESR_CIRCLE       = 0x00D0D593,
    DESR_CROSS        = 0x00E0D593,
    DESR_SQUARE       = 0x00F0D593,
    DESR_PLAY         = 0x00A0D193,
    DESR_PAUSE        = 0x0090D193,
    DESR_STOP         = 0x0080D193,
    DESR_REC_MODE     = 0x0060D293,
    DESR_REC_START    = 0x00D0D193,
    DESR_REC_PAUSE    = 0x00E0D193,
    DESR_REC_STOP     = 0x00F0D193,
    DESR_DELETE       = 0x0020D693
};

struct Key {
    uint32_t code[2] = {};
};

struct Rm {
    int port = 0;
    int initialized = 0;
    int power_mode = 0;
    int phase = 0;
    std::atomic <Key> key = Key {};
    std::atomic <uint32_t> polls = 0;

    logger::Logger* logger = nullptr;
    size_t logger_id = 0;
};

Rm* attach(logger::Logger* logger, sio2::Sio2* sio2, int port);
void set_key(Rm* rm, Key key);
void release(Rm* rm);
uint32_t next_code(Key key, int& phase);
void detach(void* udata);
void reset(void* udata);

}
