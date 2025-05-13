#ifndef CUE_H
#define CUE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdint.h>

struct disc_cue {
    FILE* file;
};

struct disc_cue* cue_create(void);
int cue_init(struct disc_cue* cue, const char* path);
void cue_destroy(struct disc_cue* cue);

// Disc IF
int cue_read_sector(void* udata, unsigned char* buf, uint64_t lba, int size);
uint64_t cue_get_size(void* udata);
uint64_t cue_get_volume_lba(void* udata);
int cue_get_sector_size(void* udata);

#ifdef __cplusplus
}
#endif

#endif