#ifndef CUE_H
#define CUE_H

#include "list.h"
#include "../disc.h"

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

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
    CUE_TRACK_READ_ERROR
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

typedef struct {
    char* name;
    int buf_mode;
    void* buf;
    size_t size;
    uint64_t start;
    list_t* tracks;
} cue_file_t;

typedef struct {
    int number;
    int mode;

    int32_t index[2];
    uint64_t pregap;
    uint64_t start;
    uint64_t end;

    cue_file_t* file;
} cue_track_t;

struct disc_cue {
    list_t* files;
    list_t* tracks;

    char c;
    FILE* file;
};

struct disc_cue* cue_create(void);
int cue_init(struct disc_cue* cue, const char* path);
void cue_destroy(struct disc_cue* cue);

// Disc interface
int cue_read(struct disc_cue* cue, uint64_t lba, void* buf, int* sector_size);
int cue_query(struct disc_cue* cue, uint64_t lba);
int cue_get_track_number_impl(struct disc_cue* cue, uint64_t lba);
int cue_get_track_count_impl(struct disc_cue* cue);
int cue_get_track_lba(struct disc_cue* cue, int track);

int cue_read_sector(void* udata, unsigned char* buf, uint64_t lba, int size);
uint64_t cue_get_size(void* udata);
uint64_t cue_get_volume_lba(void* udata);
int cue_get_sector_size(void* udata);
int cue_get_track_count(void* udata);
int cue_get_track_info(void* udata, int track, struct track_info* info);
int cue_get_track_number(void* udata, uint64_t lba);

#endif