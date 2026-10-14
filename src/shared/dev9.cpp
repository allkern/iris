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

#include "dev9.hpp"

namespace iris::dev9 {

inline constexpr uint32_t REV = 0x1f80146e;

Dev9* create(logger::Logger* logger, Model model) {
    Dev9* dev9 = new Dev9();

    dev9->logger = logger;
    dev9->logger_id = logger::register_source(logger, "dev9");

    dev9->rev = (uint16_t)model;

    return dev9;
}

void destroy(Dev9* dev9) {
    delete dev9;
}

static int reg_index(Dev9* dev9, uint32_t addr) {
    if (addr < REG_BASE || addr >= REG_BASE + (REG_COUNT * 2))
        return -1;

    return (addr - REG_BASE) >> 1;
}

static uint16_t read_reg(Dev9* dev9, uint32_t addr, int width) {
    if (addr == REV)
        return dev9->rev;

    int index = reg_index(dev9, addr);

    if (index < 0) {
        iris_warning(dev9, "Unknown {}-bit read at {:08x}", width, addr);

        return 0;
    }

    return dev9->regs[index];
}

static void write_reg(Dev9* dev9, uint32_t addr, uint16_t data, int width) {
    int index = reg_index(dev9, addr);

    if (index < 0) {
        iris_warning(dev9, "Unknown {}-bit write at {:08x} ({:04x})", width, addr, data);

        return;
    }

    dev9->regs[index] = data;
}

uint64_t read8(Dev9* dev9, uint32_t addr) {
    return read_reg(dev9, addr & ~1, 8) >> ((addr & 1) * 8) & 0xff;
}

uint64_t read16(Dev9* dev9, uint32_t addr) {
    return read_reg(dev9, addr, 16);
}

uint64_t read32(Dev9* dev9, uint32_t addr) {
    return read_reg(dev9, addr, 32);
}

void write8(Dev9* dev9, uint32_t addr, uint64_t data) {
    write_reg(dev9, addr & ~1, (uint16_t)data, 8);
}

void write16(Dev9* dev9, uint32_t addr, uint64_t data) {
    write_reg(dev9, addr, (uint16_t)data, 16);
}

void write32(Dev9* dev9, uint32_t addr, uint64_t data) {
    write_reg(dev9, addr, (uint16_t)data, 32);
}

}
