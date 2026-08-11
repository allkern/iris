#pragma once

#include "fs/fs.hpp"

namespace iris::fs::ps1mcd {

Fs* open(logger::Logger* logger, blk::Device* dev, bool take_ownership);

struct SaveInfo {
    char title[128] = {};

    int icon_frames = 0;
    uint8_t icon[3][16 * 16 * 4] = {};
};

int get_save_info(Fs* fs, const char* path, SaveInfo* out);

}
