#ifndef DISC_H
#define DISC_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define DISC_ERR_CANT_OPEN 0
#define DISC_ERR_ISO_INVALID 1
#define DISC_ERR_UNSUPPORTED_SS 2

#define DISC_EXT_ISO 0
#define DISC_EXT_BIN 1
#define DISC_EXT_CUE 2
#define DISC_EXT_NONE 3
#define DISC_EXT_UNSUPPORTED 4

#define DISC_MEDIA_CD 0
#define DISC_MEDIA_DVD 1

#define DISC_TYPE_INVALID 0
#define DISC_TYPE_CDDA 1
#define DISC_TYPE_GAME 2

/*
    00h  No disc
    01h  Detecting
    02h  Detecting CD
    03h  Detecting DVD
    04h  Detecting dual-layer DVD
    05h  Unknown
    10h  PSX CD
    11h  PSX CDDA
    12h  PS2 CD
    13h  PS2 CDDA
    14h  PS2 DVD
    FDh  CDDA (Music)
    FEh  DVDV (Movie disc)
    FFh  Illegal
*/
#define CDVD_DISC_NO_DISC          0
#define CDVD_DISC_DETECTING        1
#define CDVD_DISC_DETECTING_CD     2
#define CDVD_DISC_DETECTING_DVD    3
#define CDVD_DISC_DETECTING_DL_DVD 4
#define CDVD_DISC_PSX_CD           16
#define CDVD_DISC_PSX_CDDA         17
#define CDVD_DISC_PS2_CD           18
#define CDVD_DISC_PS2_CDDA         19
#define CDVD_DISC_PS2_DVD          20
#define CDVD_DISC_CDDA             253
#define CDVD_DISC_DVD_VIDEO        254
#define CDVD_DISC_INVALID          255

#define DISC_SS_DATA 0
#define DISC_SS_RAW 1

struct disc_state {
    int (*read_sector)(void* udata, unsigned char* buf, uint64_t lba, int size);
    uint64_t (*get_size)(void* udata);
    uint64_t (*get_volume_lba)(void* udata);
    int (*get_sector_size)(void* udata);

    void* udata;

    uint64_t layer2_lba;
    int ext;
    int pvd_cached, system_cnf_cached, root_cached, boot_path_cached;
    char pvd[2048];
    char root[2048];
    char system_cnf[2048];
    char boot_path[256];
};

struct disc_state* disc_open(const char* path);
int disc_read_sector(struct disc_state* disc, unsigned char* buf, uint64_t lba, int size);
int disc_get_type(struct disc_state* disc);
uint64_t disc_get_size(struct disc_state* disc);
uint64_t disc_get_volume_lba(struct disc_state* disc, int vol);
int disc_get_sector_size(struct disc_state* disc);
char* disc_get_serial(struct disc_state* disc, char* buf);
char* disc_get_boot_path(struct disc_state* disc);
char* disc_read_boot_elf(struct disc_state* disc, int size);
void disc_close(struct disc_state* disc);

#ifdef __cplusplus
}
#endif

#endif