#ifndef CHD_H
#define CHD_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../disc.h"

#include <libchdr/chd.h>
#include <stdio.h>
#include <stdint.h>

struct disc_chd {
    const chd_header* header;
    chd_file* file;
    uint8_t* buffer;
    size_t cached_hunknum;
    int sector_size;
};

struct disc_chd* chd_create(void);
int chd_init(struct disc_chd* chd, const char* path);
void chd_destroy(struct disc_chd* chd);

// Disc IF
int chd_read_sector(void* udata, unsigned char* buf, uint64_t lba, int size);
uint64_t chd_get_size(void* udata);
int chd_get_sector_size(void* udata);
int chd_get_track_count(void* udata);
int chd_get_track_info(void* udata, int track, struct track_info* info);
int chd_get_track_number(void* udata, uint64_t lba);

#ifdef __cplusplus
}
#endif

#endif