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

// 10C00000 - 10C07FFF: S14X SRAM (32 KB)

#pragma once
#include "logger.hpp"

namespace iris::s14x::syscon {

inline constexpr auto REG_LED = 1;
inline constexpr auto REG_SECURITY_UNLOCK = 2;
inline constexpr auto REG_RTC_FLAG = 4;
inline constexpr auto REG_WATCHDOG_FLAG2 = 5;
inline constexpr auto REG_BATTERY_LEVEL = 6;
inline constexpr auto REG_SRAM_WRITE_FLAG = 7;
inline constexpr auto REG_SECURITY_UNLOCK_SET1 = 12;
inline constexpr auto REG_SECURITY_UNLOCK_SET2 = 13;

inline constexpr auto S14X_RTC_STATE_READ_YEAR = 0;
inline constexpr auto S14X_RTC_STATE_READ_MONTH = 1;
inline constexpr auto S14X_RTC_STATE_READ_DAY = 2;
inline constexpr auto S14X_RTC_STATE_READ_DOW = 3;
inline constexpr auto S14X_RTC_STATE_READ_HOURS = 4;
inline constexpr auto S14X_RTC_STATE_READ_MINUTES = 5;
inline constexpr auto S14X_RTC_STATE_READ_SECONDS = 6;

struct Syscon {
    uint8_t led;
    uint8_t security_unlock;
    uint8_t rtc_flag;
    uint8_t battery_level;
    uint8_t watchdog_flag2;
    uint8_t security_unlock_set1;
    uint8_t security_unlock_set2;
    int sram_write_flag;

    int rtc_state;
    int rtc_bit;

    logger::Logger* logger = nullptr;
    size_t logger_id = 0;
};

Syscon* create(logger::Logger* logger);
uint64_t read8(Syscon* syscon, uint32_t addr);
void write8(Syscon* syscon, uint32_t addr, uint64_t data);
void destroy(Syscon* syscon);

}
