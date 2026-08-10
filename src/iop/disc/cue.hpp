#pragma once

#include <cstdio>

#include "../disc.hpp"


#include <vector>
#include "logger.hpp"

namespace iris::iop::disc::cue {

enum {
    TS_FAR = 0,
    TS_DATA,
    TS_AUDIO,
    TS_PREGAP
};

enum {
    CUE_OK = 0,
    CUE_FILE_NOT_FOUND,
    CUE_TRACK_FILE_NOT_FOUND,
    CUE_TRACK_READ_ERROR,
    CUE_PARSE_ERROR
};

enum {
    CUE_4CH = 0,
    CUE_AIFF,
    CUE_AUDIO,
    CUE_BINARY,
    CUE_CATALOG,
    CUE_CDG,
    CUE_CDI_2336,
    CUE_CDI_2352,
    CUE_CDTEXTFILE,
    CUE_DCP,
    CUE_FILE,
    CUE_FLAGS,
    CUE_INDEX,
    CUE_ISRC,
    CUE_MODE1_2048,
    CUE_MODE1_2352,
    CUE_MODE2_2336,
    CUE_MODE2_2352,
    CUE_MOTOROLA,
    CUE_MP3,
    CUE_PERFORMER,
    CUE_POSTGAP,
    CUE_PRE,
    CUE_PREGAP,
    CUE_REM,
    CUE_SCMS,
    CUE_SONGWRITER,
    CUE_TITLE,
    CUE_TRACK,
    CUE_WAVE,
    CUE_NONE = 255
};

enum {
    LD_BUFFERED,
    LD_FILE
};

struct Track;
struct File {
    char* name;
    int buf_mode;
    void* buf;
    size_t size;
    uint64_t start;
    std::vector <Track*> tracks;
};

struct Track {
    int number;
    int mode;

    int32_t index[2];
    uint64_t pregap;
    uint64_t start;
    uint64_t end;

    File* file;
};

struct Cue {
    std::vector <File*> files;
    std::vector <Track*> tracks;

    char c;
    FILE* file;

    logger::Logger* logger = nullptr;
    size_t logger_id = 0;
};

Cue* create(logger::Logger* logger);
int init(Cue* cue, const char* path);
void destroy(Cue* cue);

// Disc interface
int read(Cue* cue, uint64_t lba, void* buf, int* sector_size);
int query(Cue* cue, uint64_t lba);
int get_track_number_impl(Cue* cue, uint64_t lba);
int get_track_count_impl(Cue* cue);
int get_track_lba(Cue* cue, int track);

int read_sector(void* udata, unsigned char* buf, uint64_t lba, int size);
uint64_t get_size(void* udata);
int get_sector_size(void* udata);
int get_track_count(void* udata);
int get_track_info(void* udata, int track, TrackInfo* info);
int get_track_number(void* udata, uint64_t lba);

}
