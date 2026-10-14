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

#include <cstdint>

#include "logger.hpp"

namespace iris::ps2 {

struct Ps2;

enum TtyType {
    EE,
    IOP,
    SYSMEM
};

inline constexpr auto EE_CLOCK_CONSOLE = 294912000;
inline constexpr auto EE_CLOCK_SYSTEM_256 = 393216000;
inline constexpr auto EE_CLOCK_SUPER_256 = 442368000;

enum SystemType {
    AUTO = 0,
    RETAIL,
    RETAIL_DRAGON,
    PSX_DESR,
    TEST,
    TOOL,
    KONAMI_PYTHON,
    KONAMI_PYTHON2,
    NAMCO_SYSTEM_147,
    NAMCO_SYSTEM_148,
    NAMCO_SYSTEM_246,
    NAMCO_SYSTEM_256,
    NAMCO_SYSTEM_SUPER_256,
    WEGA_HVX
};

Ps2* create(logger::Logger* logger);
void init(Ps2* ps2);
void init_tty_handler(Ps2* ps2, TtyType tty, void (*handler)(void*, char), void* udata);
void iop_map_device(Ps2* ps2, const char* device, const char* host_path);
void iop_unmap_device(Ps2* ps2, const char* device);
void iop_clear_device_maps(Ps2* ps2);
void boot_file(Ps2* ps2, const char* path);
void set_boot_args(Ps2* ps2, const char* const* args, int count);
void reset(Ps2* ps2);
int load_bios(Ps2* ps2, const char* path);
int load_rom1(Ps2* ps2, const char* path);
int load_rom2(Ps2* ps2, const char* path);
void cycle(Ps2* ps2);
void step_ee(Ps2* ps2);
void step_iop(Ps2* ps2);
void set_timescale(Ps2* ps2, int timescale);
void destroy(Ps2* ps2);
void set_system(Ps2* ps2, int system);
void set_mac_address(Ps2* ps2, const uint8_t* mac);

}
