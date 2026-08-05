#pragma once

#include "../disc.hpp"
#include "logger.hpp"

namespace iris::iop::disc::bin {

struct Bin {
    FILE* file;

    logger::Logger* logger = nullptr;
    size_t logger_id = 0;
};

Bin* create(logger::Logger* logger);
int init(Bin* bin, const char* path);
void destroy(Bin* bin);

// Disc IF
int read_sector(void* udata, unsigned char* buf, uint64_t lba, int size);
uint64_t get_size(void* udata);
int get_sector_size(void* udata);
int get_track_count(void* udata);
int get_track_info(void* udata, int track, TrackInfo* info);
int get_track_number(void* udata, uint64_t lba);

}
