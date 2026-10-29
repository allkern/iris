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

#include "disc.hpp"

#include "shared/speed/ata.hpp"

namespace iris::ata::disc {

bool is_compressed(const char* path) {
    int ext = iop::disc::get_extension(path);

    return ext == iop::disc::DISC_EXT_CHD ||
           ext == iop::disc::DISC_EXT_CSO ||
           ext == iop::disc::DISC_EXT_ZSO ||
           ext == iop::disc::DISC_EXT_MDS ||
           ext == iop::disc::DISC_EXT_MDF;
}

iop::disc::Disc* open(logger::Logger* logger, const char* path) {
    return iop::disc::open(logger, path);
}

void ata_read_sector(void* udata, uint64_t lba, uint8_t* buf) {
    iop::disc::read_sector((iop::disc::Disc*)udata, buf, lba, iop::disc::DISC_SS_DATA);
}

void ata_write_sector(void* udata, uint64_t lba, const uint8_t* buf) {
    // Compressed images are read only
}

int ata_get_identify(void* udata, uint8_t* buf) {
    return 0;
}

uint64_t ata_get_sector_count(void* udata) {
    iop::disc::Disc* disc = (iop::disc::Disc*)udata;

    int sector_size = iop::disc::get_sector_size(disc);

    if (sector_size <= 0)
        sector_size = speed::ata::SECTOR_SIZE;

    return iop::disc::get_size(disc) / sector_size;
}

void ata_close(void* udata) {
    iop::disc::close((iop::disc::Disc*)udata);
}

}
