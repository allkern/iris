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

#include <cstdio>
#include <cstdint>
#include <vector>

#include "../disc.hpp"
#include "logger.hpp"

namespace iris::iop::disc::mds {

struct Track {
    int number;
    int type;
    uint32_t pregap;
    uint32_t start;
    uint32_t length;
    uint64_t offset;
    int stride;
    int sector_size;
    int data_offset;
};

struct Mds {
    FILE* file = nullptr;
    int media = DISC_MEDIA_CD;
    uint64_t layer_break = 0;
    std::vector <Track> tracks;

    logger::Logger* logger = nullptr;
    size_t logger_id = 0;
};

Mds* create(logger::Logger* logger);
int init(Mds* mds, const char* path);
void destroy(Mds* mds);

int read_sector(void* udata, unsigned char* buf, uint64_t lba, int size);
uint64_t get_size(void* udata);
int get_sector_size(void* udata);
int get_track_count(void* udata);
int get_track_info(void* udata, int track, TrackInfo* info);
int get_track_number(void* udata, uint64_t lba);

}
