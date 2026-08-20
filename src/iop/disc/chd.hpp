#pragma once

#include "../disc.hpp"

#include <libchdr/chd.h>
#include "logger.hpp"

namespace iris::iop::disc::chd {

inline constexpr auto NO_CACHED_HUNK = (size_t)-1;

struct Chd {
    const chd_header* header;
    chd_file* file;
    uint8_t* buffer;
    size_t cached_hunknum = NO_CACHED_HUNK;
    int sector_size;
    int is_disc;

    logger::Logger* logger = nullptr;
    size_t logger_id = 0;
};

Chd* create(logger::Logger* logger);
int init(Chd* chd, const char* path);
void destroy(Chd* chd);

// Disc IF
int read_sector(void* udata, unsigned char* buf, uint64_t lba, int size);
uint64_t get_size(void* udata);
int get_sector_size(void* udata);
int get_track_count(void* udata);
int get_track_info(void* udata, int track, TrackInfo* info);
int get_track_number(void* udata, uint64_t lba);

}
