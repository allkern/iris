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

#include <new>
#include "sram.hpp"

namespace iris::s14x::sram {

Sram* create(logger::Logger* logger, int* write_flag) {
    Sram* sram = new Sram();

    sram->logger = logger;
    sram->logger_id = logger::register_source(logger, "sram");

    sram->write_flag = write_flag;

    return sram;
}

int load(Sram* sram, const char* path) {
    FILE* file = fopen(path, "rb");

    sram->path = path;

    // If the file doesn't exist then we'll create it
    if (!file) {
        file = fopen(path, "wb");

        if (!file) {
            iris_error(sram, "Couldn't create SRAM file \"{}\"", path);

            return 0;
        }

        fwrite(sram->buf, 1, SIZE, file);
        fclose(file);

        return 0;
    }

    fread(sram->buf, 1, SIZE, file);
    fclose(file);

    return 1;
}

uint64_t read8(Sram* sram, uint32_t addr) {
    return sram->buf[addr & 0x7fff];
}

uint64_t read16(Sram* sram, uint32_t addr) {
    return *(uint16_t*)(sram->buf + (addr & 0x7fff));
}

uint64_t read32(Sram* sram, uint32_t addr) {
    return *(uint32_t*)(sram->buf + (addr & 0x7fff));
}

void write8(Sram* sram, uint32_t addr, uint64_t data) {
    if (sram->write_flag && !*sram->write_flag) return;

    sram->buf[addr & 0x7fff] = data & 0xff;
}

void write16(Sram* sram, uint32_t addr, uint64_t data) {
    if (sram->write_flag && !*sram->write_flag) return;

    *(uint16_t*)(sram->buf + (addr & 0x7fff)) = data & 0xffff;
}

void write32(Sram* sram, uint32_t addr, uint64_t data) {
    if (sram->write_flag && !*sram->write_flag) return;

    *(uint32_t*)(sram->buf + (addr & 0x7fff)) = data & 0xffffffff;
}

void destroy(Sram* sram) {
    if (sram->path.size()) {
        FILE* file = fopen(sram->path.c_str(), "wb");

        if (file) {
            fwrite(sram->buf, 1, SIZE, file);
            fclose(file);
        } else {
            iris_error(sram, "Couldn't write SRAM back to \"{}\"", sram->path);
        }
    }

    delete sram;
}

}
