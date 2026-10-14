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
#include <cstdio>

#include "logger.hpp"

namespace iris::ata::raw {

struct Raw {
    FILE* file = nullptr;

    uint64_t sector_count = 0;

    logger::Logger* logger = nullptr;
    size_t logger_id = 0;
};

Raw* open(logger::Logger* logger, const char* path);
uint64_t get_sector_count(Raw* raw);
void read_sector(Raw* raw, uint64_t lba, uint8_t* buf);
void write_sector(Raw* raw, uint64_t lba, const uint8_t* buf);
int get_identify(Raw* raw, uint8_t* buf);
void close(Raw* raw);

// ATA adapters
void ata_read_sector(void* udata, uint64_t lba, uint8_t* buf);
void ata_write_sector(void* udata, uint64_t lba, const uint8_t* buf);
int ata_get_identify(void* udata, uint8_t* buf);
uint64_t ata_get_sector_count(void* udata);
void ata_close(void* udata);

}
