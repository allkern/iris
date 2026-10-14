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

#include "iso.hpp"

namespace iris::iop::disc::iso {

Iso* create(logger::Logger* logger) {
    Iso* iso = new Iso();

    iso->logger = logger;
    iso->logger_id = logger::register_source(logger, "iso");

    return iso;
}

int init(Iso* iso, const char* path) {
    iso->file = fopen(path, "rb");

    if (!iso->file) {
        delete iso;

        return 0;
    }

    return 1;
}

void destroy(Iso* iso) {
    fclose(iso->file);
    delete iso;
}

// Disc IF
int read_sector(void* udata, unsigned char* buf, uint64_t lba, int size) {
    Iso* iso = (Iso*)udata;

    int s, r;

    s = fseek64(iso->file, lba * 0x800, SEEK_SET);
    r = fread(buf, 1, 0x800, iso->file);

    return r && !s;
}

uint64_t get_size(void* udata) {
    Iso* iso = (Iso*)udata;

    fseek64(iso->file, 0, SEEK_END);

    return ftell64(iso->file);
}

int get_sector_size(void* udata) {
    return 2048;
}

int get_track_count(void* udata) {
    return 1;
}

int get_track_info(void* udata, int track, TrackInfo* info) {
    return 0;
}

int get_track_number(void* udata, uint64_t lba) {
    return 1;
}

#undef fseek64
#undef ftell64

}
