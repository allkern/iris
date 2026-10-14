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

#include "acsram.hpp"

namespace iris::s2x6::acsram {

Acsram* create(logger::Logger* logger) {
    Acsram* acsram = new Acsram();

    acsram->logger = logger;
    acsram->logger_id = logger::register_source(logger, "acsram");

    return acsram;
}

int load(Acsram* acsram, const char* path) {
    acsram->path = path;

    FILE* file = fopen(path, "rb");

    if (!file)
        return 0;

    fread(acsram->buf, 1, SIZE, file);
    fclose(file);

    iris_debug(acsram, "Loaded settings from '{}'", path);

    return 1;
}

uint64_t read8(Acsram* acsram, uint32_t addr) {
    return acsram->buf[(addr >> 1) & (SIZE - 1)];
}

uint64_t read16(Acsram* acsram, uint32_t addr) {
    return acsram->buf[(addr >> 1) & (SIZE - 1)];
}

void write16(Acsram* acsram, uint32_t addr, uint64_t data) {
    uint32_t offset = (addr >> 1) & (SIZE - 1);
    uint32_t region = offset >> 10;

    if (!acsram->written_regions[region]) {
        acsram->written_regions[region] = 1;

        iris_debug(acsram, "Write {:04x} = {:02x}", offset, (uint8_t)data);
    }

    acsram->buf[offset] = data & 0xff;
}

void destroy(Acsram* acsram) {
    if (acsram->path.size()) {
        FILE* file = fopen(acsram->path.c_str(), "wb");

        if (file) {
            fwrite(acsram->buf, 1, SIZE, file);
            fclose(file);
        }
    }

    delete acsram;
}
}
