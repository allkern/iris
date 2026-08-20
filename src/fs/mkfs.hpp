#pragma once

#include "fs/fs.hpp"

namespace iris::fs::mkfs {

enum : int {
    MKFS_PS2_MCD,
    MKFS_PS1_MCD,
    MKFS_FAT32,
    MKFS_EXFAT
};

struct Params {
    int type = MKFS_FAT32;
    uint64_t size = 0;
    bool partition = false;
    bool ecc = true;

    char label[64] = {};
};

inline constexpr uint32_t PAGE_SIZE = 512;
inline constexpr uint32_t PAGE_ECC_SIZE = 16;

int create_image(logger::Logger* logger, const char* path, uint64_t size, uint8_t fill);
int format(logger::Logger* logger, const char* path, const Params& params);
void page_ecc(const uint8_t* page, uint8_t* out);

}
