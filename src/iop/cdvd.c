#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <ctype.h>
#include <time.h>

#include "cdvd.h"

#define printf(fmt,...)(0)

struct nvram_layout g_spc970_layout = {
    .bios_version = 0x00000000,
    .config0_offset = 0x00000280,
    .config1_offset = 0x00000300,
    .config2_offset = 0x00000200,
    .console_id_offset = 0x000001C8,
    .ilink_id_offset = 0x000001C0,
    .modelnum_offset = 0x000001A0,
    .regparams_offset = 0x00000180,
    .mac_offset = 0x00000198
};

struct nvram_layout g_dragon_layout = {
    .bios_version = 0x00000146,
    .config0_offset = 0x00000270,
    .config1_offset = 0x000002B0,
    .config2_offset = 0x00000200,
    .console_id_offset = 0x000001F0,
    .ilink_id_offset = 0x000001E0,
    .modelnum_offset = 0x000001B0,
    .regparams_offset = 0x00000180,
    .mac_offset = 0x00000198
};

static const uint8_t nvram_init_data[1024] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0xAC, 0xFF, 0xFF, 0xFF, 0xFF, 0xB9, 0x86,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x30, 0x21, 0x00, 0x00, 0x00, 0x70, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x41, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00
};

static const uint8_t itob_table[] = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
    0x08, 0x09, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15,
    0x16, 0x17, 0x18, 0x19, 0x20, 0x21, 0x22, 0x23,
    0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x30, 0x31,
    0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39,
    0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47,
    0x48, 0x49, 0x50, 0x51, 0x52, 0x53, 0x54, 0x55,
    0x56, 0x57, 0x58, 0x59, 0x60, 0x61, 0x62, 0x63,
    0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x70, 0x71,
    0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79,
    0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,
    0x88, 0x89, 0x90, 0x91, 0x92, 0x93, 0x94, 0x95,
    0x96, 0x97, 0x98, 0x99, 0xa0, 0xa1, 0xa2, 0xa3,
    0xa4, 0xa5, 0xa6, 0xa7, 0xa8, 0xa9, 0xb0, 0xb1,
    0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7, 0xb8, 0xb9,
    0xc0, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7,
    0xc8, 0xc9, 0xd0, 0xd1, 0xd2, 0xd3, 0xd4, 0xd5,
    0xd6, 0xd7, 0xd8, 0xd9, 0xe0, 0xe1, 0xe2, 0xe3,
    0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xf0, 0xf1,
    0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9,
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
    0x08, 0x09, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15,
    0x16, 0x17, 0x18, 0x19, 0x20, 0x21, 0x22, 0x23,
    0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x30, 0x31,
    0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39,
    0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47,
    0x48, 0x49, 0x50, 0x51, 0x52, 0x53, 0x54, 0x55,
    0x56, 0x57, 0x58, 0x59, 0x60, 0x61, 0x62, 0x63,
    0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x70, 0x71,
    0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79,
    0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,
    0x88, 0x89, 0x90, 0x91, 0x92, 0x93, 0x94, 0x95,
};

static inline const char* cdvd_get_n_command_name(uint8_t cmd) {
    switch (cmd) {
        case 0x00: return "nop";
        case 0x01: return "nop_sync";
        case 0x02: return "standby";
        case 0x03: return "stop";
        case 0x04: return "pause";
        case 0x05: return "seek";
        case 0x06: return "read_cd";
        case 0x07: return "read_cdda";
        case 0x08: return "read_dvd";
        case 0x09: return "get_toc";
        case 0x0c: return "read_key";
    }

    return "<unknown>";
}

static inline const char* cdvd_get_type_name(int type) {
    switch (type) {
        case CDVD_DISC_NO_DISC: return "No disc";
        case CDVD_DISC_DETECTING: return "Detecting";
        case CDVD_DISC_DETECTING_CD: return "Detecting CD";
        case CDVD_DISC_DETECTING_DVD: return "Detecting DVD";
        case CDVD_DISC_DETECTING_DL_DVD: return "Detecting Dual-layer DVD";
        case CDVD_DISC_PSX_CD: return "PlayStation CD";
        case CDVD_DISC_PSX_CDDA: return "PlayStation CDDA";
        case CDVD_DISC_PS2_CD: return "PlayStation 2 CD";
        case CDVD_DISC_PS2_CDDA: return "PlayStation 2 CDDA";
        case CDVD_DISC_PS2_DVD: return "PlayStation 2 DVD";
        case CDVD_DISC_CDDA: return "CD Audio";
        case CDVD_DISC_DVD_VIDEO: return "DVD Video";
        case CDVD_DISC_INVALID: return "Invalid";
    }

    return "Unknown";
}

static inline int cdvd_is_dual_layer(struct ps2_cdvd* cdvd) {
    return disc_get_volume_lba(cdvd->disc, 1);
}

static inline void cdvd_set_busy(struct ps2_cdvd* cdvd) {
    cdvd->n_stat |= CDVD_N_STATUS_BUSY;
    cdvd->n_stat &= ~CDVD_N_STATUS_READY;
}

static inline void cdvd_set_ready(struct ps2_cdvd* cdvd) {
    cdvd->n_stat &= ~CDVD_N_STATUS_BUSY;
    cdvd->n_stat |= CDVD_N_STATUS_READY;
}

static inline void cdvd_init_s_fifo(struct ps2_cdvd* cdvd, int size) {
    if (cdvd->s_fifo)
        free(cdvd->s_fifo);

    cdvd->s_fifo_size = size;
    cdvd->s_fifo_index = 0;
    cdvd->s_fifo = malloc(cdvd->s_fifo_size);
    cdvd->s_stat &= ~0x40;
}

static inline void cdvd_s_read_subq(struct ps2_cdvd* cdvd) {
    cdvd_init_s_fifo(cdvd, 11);

    int track = disc_get_track_number(cdvd->disc, cdvd->read_lba);

    struct track_info info;
    disc_get_track_info(cdvd->disc, track, &info);

    int track_lba = (cdvd->read_lba + 150) - info.lba;

    int track_mm = track_lba / (60 * 75);
    int track_ss = (track_lba % (60 * 75)) / 75;
    int track_ff = (track_lba % (60 * 75)) % 75;
    int abs_mm = (cdvd->read_lba + 150) / (60 * 75);
    int abs_ss = ((cdvd->read_lba + 150) % (60 * 75)) / 75;
    int abs_ff = ((cdvd->read_lba + 150) % (60 * 75)) % 75;

    fprintf(stdout, "cdvd: S subq read: track=%02d trk %02d:%02d:%02d abs %02d:%02d:%02d\n",
        track,
        track_mm, track_ss, track_ff,
        abs_mm, abs_ss, abs_ff
    );

    // Note: From PCSX2
    //       the formatted subq command returns: 
    //       control/adr, track, index, trk min, trk sec, trk frm, 0x00, abs min, abs sec, abs frm
    memset(&cdvd->s_fifo[0], 0, 11);

    cdvd->s_fifo[0] = 0x41; // control/adr
    cdvd->s_fifo[1] = track; // track
    cdvd->s_fifo[2] = 0x01; // index
    cdvd->s_fifo[3] = itob_table[track_mm]; // trk min
    cdvd->s_fifo[4] = itob_table[track_ss]; // trk sec
    cdvd->s_fifo[5] = itob_table[track_ff]; // trk frm
    cdvd->s_fifo[6] = 0x00;
    cdvd->s_fifo[7] = itob_table[abs_mm]; // abs min
    cdvd->s_fifo[8] = itob_table[abs_ss]; // abs sec
    cdvd->s_fifo[9] = itob_table[abs_ff]; // abs frm

    // To-do: This doesn't work for whatever reason, even though
    //        we're returning correct data, the CD player just
    //        won't actually display the current time.
}
static inline void cdvd_s_mechacon_cmd(struct ps2_cdvd* cdvd) {
    switch (cdvd->s_params[0]) {
        case 0x00: {
            cdvd_init_s_fifo(cdvd, 4);

            cdvd->s_fifo[0] = 0x03;
            cdvd->s_fifo[1] = 0x06;
            cdvd->s_fifo[2] = 0x02;
            cdvd->s_fifo[3] = 0x00;
        } break;

        case 0x90: {
            cdvd_init_s_fifo(cdvd, 1);

            cdvd->s_fifo[0] = 0x00;
        } break;

        case 0xef: {
            cdvd_init_s_fifo(cdvd, 3);

            cdvd->s_fifo[0] = 0x00;
            cdvd->s_fifo[1] = 0x0f;
            cdvd->s_fifo[2] = 0x05;
        } break;

        // sceCdReadRenewalDate (sent by PSX DESR BIOS)
        case 0xfd: {
            cdvd_init_s_fifo(cdvd, 6);

            cdvd->s_fifo[0] = 0;
            cdvd->s_fifo[1] = 0x04; //year
            cdvd->s_fifo[2] = 0x12; //month
            cdvd->s_fifo[3] = 0x10; //day
            cdvd->s_fifo[4] = 0x01; //hour
            cdvd->s_fifo[5] = 0x30; //min
        } break;

        default: {
            fprintf(stderr, "cdvd: Unknown S subcommand %02x\n", cdvd->s_params[0]);

            exit(1);
        } break;
    }
}
static inline void cdvd_s_update_sticky_flags(struct ps2_cdvd* cdvd) {
    cdvd_init_s_fifo(cdvd, 1);

    cdvd->s_fifo[0] = 0;

    cdvd->sticky_status = cdvd->status;
}
static inline void cdvd_s_read_rtc(struct ps2_cdvd* cdvd) {
    time_t t = time(NULL);

    struct tm tm = *localtime(&t);

    cdvd_init_s_fifo(cdvd, 8);

    cdvd->s_fifo[0] = 0;
    cdvd->s_fifo[1] = itob_table[tm.tm_sec];
    cdvd->s_fifo[2] = itob_table[tm.tm_min];
    cdvd->s_fifo[3] = itob_table[tm.tm_hour];
    cdvd->s_fifo[4] = 0;
    cdvd->s_fifo[5] = itob_table[tm.tm_mday];
    cdvd->s_fifo[6] = itob_table[tm.tm_mon + 1];
    cdvd->s_fifo[7] = itob_table[tm.tm_year - 100];
}
static inline void cdvd_s_write_rtc(struct ps2_cdvd* cdvd) {
    fprintf(stderr, "cdvd: write_rtc\n");

    exit(1);
}
static inline void cdvd_s_read_nvram(struct ps2_cdvd* cdvd) {
    uint16_t addr = *(uint16_t*)&cdvd->s_params[0];

    cdvd_init_s_fifo(cdvd, 2);

    cdvd->s_fifo[0] = cdvd->nvram[(addr << 1) + 0];
    cdvd->s_fifo[1] = cdvd->nvram[(addr << 1) + 1];
}
static inline void cdvd_s_write_nvram(struct ps2_cdvd* cdvd) {
    fprintf(stderr, "cdvd: write_nvram\n");

    exit(1);
}
static inline void cdvd_s_read_ilink_id(struct ps2_cdvd* cdvd) {
    cdvd_init_s_fifo(cdvd, 9);

    // uint8_t id[9] = {
    //     0x00, 0xac, 0xff, 0xff,
    //     0xff, 0xff, 0xb9, 0x86,
    //     0x00
    // };

    // for (int i = 0; i < 9; i++) {
    //     cdvd->s_fifo[i] = id[i];
    // }

    int offset = g_dragon_layout.ilink_id_offset;

    memcpy(&cdvd->s_fifo[1], &cdvd->nvram[offset], 8);
}
static inline void cdvd_s_ctrl_audio_digital_out(struct ps2_cdvd* cdvd) {
    cdvd_init_s_fifo(cdvd, 1);

    cdvd->s_fifo[0] = 0;
}
static inline void cdvd_s_forbid_dvd(struct ps2_cdvd* cdvd) {
    cdvd_init_s_fifo(cdvd, 1);

    cdvd->s_fifo[0] = 5;
}
static inline void cdvd_s_read_model(struct ps2_cdvd* cdvd) {
    cdvd_init_s_fifo(cdvd, 9);

    int offset = g_dragon_layout.modelnum_offset + cdvd->s_params[0];

    memcpy(&cdvd->s_fifo[1], &cdvd->nvram[offset], 8);
}
static inline void cdvd_s_certify_boot(struct ps2_cdvd* cdvd) {
    cdvd_init_s_fifo(cdvd, 1);

    cdvd->s_fifo[0] = 1;
}
static inline void cdvd_s_cancel_pwoff_ready(struct ps2_cdvd* cdvd) {
    cdvd_init_s_fifo(cdvd, 1);

    cdvd->s_fifo[0] = 0;
}
static inline void cdvd_s_blue_led_ctl(struct ps2_cdvd* cdvd) {
    cdvd_init_s_fifo(cdvd, 1);

    cdvd->s_fifo[0] = 0;
}
static inline void cdvd_s_read_wakeup_time(struct ps2_cdvd* cdvd) {
    cdvd_init_s_fifo(cdvd, 10);

    for (int i = 0; i < 10; i++)
        cdvd->s_fifo[i] = 0;
}
static inline void cdvd_s_rc_bypass_ctrl(struct ps2_cdvd* cdvd) {
    cdvd_init_s_fifo(cdvd, 1);

    cdvd->s_fifo[0] = 0;
}
static inline void cdvd_s_open_config(struct ps2_cdvd* cdvd) {
    cdvd_init_s_fifo(cdvd, 1);

    cdvd->config_rw = cdvd->s_params[0];
    cdvd->config_offset = cdvd->s_params[1];
    cdvd->config_numblocks = cdvd->s_params[2];
    cdvd->config_block_index = 0;

    cdvd->s_fifo[0] = 0;
}
static inline void cdvd_s_read_config(struct ps2_cdvd* cdvd) {
    cdvd_init_s_fifo(cdvd, 16);

    int offset = 0;

    switch (cdvd->config_offset) {
        case 0: offset = g_dragon_layout.config0_offset; break;
        case 1: offset = g_dragon_layout.config1_offset; break;
        case 2: offset = g_dragon_layout.config2_offset; break;

        default: offset = g_dragon_layout.config1_offset; break;
    }

    offset += (cdvd->config_block_index++) * 16;

    memcpy(cdvd->s_fifo, &cdvd->nvram[offset], 16);
}
static inline void cdvd_s_write_config(struct ps2_cdvd* cdvd) {
    cdvd_init_s_fifo(cdvd, 1);

    cdvd->s_fifo[0] = 0;
}
static inline void cdvd_s_close_config(struct ps2_cdvd* cdvd) {
    cdvd_init_s_fifo(cdvd, 1);

    cdvd->s_fifo[0] = 0;
}
static inline void cdvd_s_mechacon_auth_80(struct ps2_cdvd* cdvd) {
    cdvd_init_s_fifo(cdvd, 1);

    cdvd->s_fifo[0] = 0;
}
static inline void cdvd_s_mechacon_auth_81(struct ps2_cdvd* cdvd) {
    cdvd_init_s_fifo(cdvd, 1);

    cdvd->s_fifo[0] = 0;
}
static inline void cdvd_s_mechacon_auth_82(struct ps2_cdvd* cdvd) {
    cdvd_init_s_fifo(cdvd, 1);

    cdvd->s_fifo[0] = 0;
}
static inline void cdvd_s_mechacon_auth_83(struct ps2_cdvd* cdvd) {
    cdvd_init_s_fifo(cdvd, 1);

    cdvd->s_fifo[0] = 0;
}
static inline void cdvd_s_mechacon_auth_84(struct ps2_cdvd* cdvd) {
    cdvd_init_s_fifo(cdvd, 1+8+4);

    cdvd->s_fifo[0] = 0;
    cdvd->s_fifo[1] = 0x21;
    cdvd->s_fifo[2] = 0xdc;
    cdvd->s_fifo[3] = 0x31;
    cdvd->s_fifo[4] = 0x96;
    cdvd->s_fifo[5] = 0xce;
    cdvd->s_fifo[6] = 0x72;
    cdvd->s_fifo[7] = 0xe0;
    cdvd->s_fifo[8] = 0xc8;
    cdvd->s_fifo[9]  = 0x69;
    cdvd->s_fifo[10] = 0xda;
    cdvd->s_fifo[11] = 0x34;
    cdvd->s_fifo[12] = 0x9b;
}
static inline void cdvd_s_mechacon_auth_85(struct ps2_cdvd* cdvd) {
    cdvd_init_s_fifo(cdvd, 1+4+8);

    cdvd->s_fifo[0] = 0;
    cdvd->s_fifo[1] = 0xeb;
    cdvd->s_fifo[2] = 0x01;
    cdvd->s_fifo[3] = 0xc7;
    cdvd->s_fifo[4] = 0xa9;
    cdvd->s_fifo[5] = 0x3f;
    cdvd->s_fifo[6] = 0x9c;
    cdvd->s_fifo[7] = 0x5b;
    cdvd->s_fifo[8] = 0x19;
    cdvd->s_fifo[9] = 0x31;
    cdvd->s_fifo[10] = 0xa0;
    cdvd->s_fifo[11] = 0xb3;
    cdvd->s_fifo[12] = 0xa3;
}
static inline void cdvd_s_mechacon_auth_86(struct ps2_cdvd* cdvd) {
    cdvd_init_s_fifo(cdvd, 1);

    cdvd->s_fifo[0] = 0;
}
static inline void cdvd_s_mechacon_auth_87(struct ps2_cdvd* cdvd) {
    cdvd_init_s_fifo(cdvd, 1);

    cdvd->s_fifo[0] = 0;
}
static inline void cdvd_s_mechacon_auth_88(struct ps2_cdvd* cdvd) {
    cdvd_init_s_fifo(cdvd, 1);

    cdvd->s_fifo[0] = 0;
}
static inline void cdvd_s_mg_write_data(struct ps2_cdvd* cdvd) {
    cdvd_init_s_fifo(cdvd, 1);

    cdvd->s_fifo[0] = 0;

    printf("mg: Write KELF data params=");

    for (int i = 0; i < cdvd->s_param_index; i++)
        printf("%02x ", cdvd->s_params[i]);

    printf("\n");
}
static inline void cdvd_s_mechacon_auth_8f(struct ps2_cdvd* cdvd) {
    cdvd_init_s_fifo(cdvd, 1);

    cdvd->s_fifo[0] = 0;
}
static inline void cdvd_s_mg_write_hdr_start(struct ps2_cdvd* cdvd) {
    cdvd_init_s_fifo(cdvd, 1);

    printf("mg: Write KELF header params=");

    for (int i = 0; i < cdvd->s_param_index; i++)
        printf("%02x ", cdvd->s_params[i]);

    printf("\n");

    cdvd->s_fifo[0] = 0;
}
static inline void cdvd_s_mg_read_bit_length(struct ps2_cdvd* cdvd) {
    cdvd_init_s_fifo(cdvd, 3);

    for (int i = 0; i < 3; i++)
        cdvd->s_fifo[i] = 0;
}
static inline void cdvd_s_get_region_params(struct ps2_cdvd* cdvd) {
    cdvd_init_s_fifo(cdvd, 15);

    for (int i = 5; i < 15; i++)
        cdvd->s_fifo[i] = 0;

    int offset = g_dragon_layout.regparams_offset;

    cdvd->s_fifo[1] = 1 << 0; // MechaCon encryption zone
    cdvd->s_fifo[2] = 0;

    memcpy(&cdvd->s_fifo[3], &cdvd->nvram[offset], 8);

    fprintf(stdout, "cdvd: Region: \'%c\' Language set: %c%c%c%c\n",
        cdvd->s_fifo[3],
        cdvd->s_fifo[4],
        cdvd->s_fifo[5],
        cdvd->s_fifo[6],
        cdvd->s_fifo[7]
    );

    //This is basically what PCSX2 returns on a blank NVM/MEC file
    // cdvd->s_fifo[0] = 0;
    // cdvd->s_fifo[1] = 1 << 0x3; //MEC encryption zone
    // cdvd->s_fifo[2] = 0;
    // cdvd->s_fifo[3] = 0x80; //Region Params
    // cdvd->s_fifo[4] = 0x1;
}
static inline void cdvd_s_remote2_read(struct ps2_cdvd* cdvd) {
    cdvd_init_s_fifo(cdvd, 5);

    cdvd->s_fifo[0] = 0x00;
    cdvd->s_fifo[1] = 0x14;
    cdvd->s_fifo[2] = 0x00;
    cdvd->s_fifo[3] = 0x00;
    cdvd->s_fifo[4] = 0x00;
}
static inline void cdvd_s_auto_adjust_ctrl(struct ps2_cdvd* cdvd) {
    cdvd_init_s_fifo(cdvd, 1);

    cdvd->s_fifo[0] = 0;
}
static inline void cdvd_s_notice_game_start(struct ps2_cdvd* cdvd) {
    cdvd_init_s_fifo(cdvd, 1);

    cdvd->s_fifo[0] = 0;
}
static inline void cdvd_s_get_medium_removal(struct ps2_cdvd* cdvd) {
    cdvd_init_s_fifo(cdvd, 2);

    cdvd->s_fifo[0] = 0;
}

void cdvd_handle_s_command(struct ps2_cdvd* cdvd, uint8_t cmd) {
    cdvd->s_cmd = cmd;

    switch (cmd) {
        // Note: Used by CD player to get current playing position
        case 0x02: printf("cdvd: read_subq\n"); cdvd_s_read_subq(cdvd); break;
        case 0x03: printf("cdvd: mechacon_cmd(%02x)\n", cdvd->s_params[0]); cdvd_s_mechacon_cmd(cdvd); break;
        case 0x05: printf("cdvd: update_sticky_flags\n"); cdvd_s_update_sticky_flags(cdvd); break;
        // case 0x06: printf("cdvd: tray_ctrl\n"); cdvd_s_tray_ctrl(cdvd); break;
        case 0x08: printf("cdvd: read_rtc\n"); cdvd_s_read_rtc(cdvd); break;
        case 0x09: printf("cdvd: write_rtc\n"); cdvd_s_write_rtc(cdvd); break;
        case 0x0a: printf("cdvd: read_nvram\n"); cdvd_s_read_nvram(cdvd); break;
        case 0x0b: printf("cdvd: write_nvram\n"); cdvd_s_write_nvram(cdvd); break;
        // case 0x0f: printf("cdvd: power_off\n"); cdvd_s_power_off(cdvd); break;
        case 0x12: printf("cdvd: read_ilink_id\n"); cdvd_s_read_ilink_id(cdvd); break;

        // Note: Used by CD player
        case 0x14: printf("cdvd: ctrl_audio_digital_out\n"); cdvd_s_ctrl_audio_digital_out(cdvd); break;
        case 0x15: printf("cdvd: forbid_dvd\n"); cdvd_s_forbid_dvd(cdvd); break;
        case 0x16: printf("cdvd: auto_adjust_ctrl\n"); cdvd_s_auto_adjust_ctrl(cdvd); break;
        case 0x17: printf("cdvd: read_model\n"); cdvd_s_read_model(cdvd); break;
        case 0x1a: printf("cdvd: certify_boot\n"); cdvd_s_certify_boot(cdvd); break;
        case 0x1b: printf("cdvd: cancel_pwoff_ready\n"); cdvd_s_cancel_pwoff_ready(cdvd); break;

        // Used by Namco System 246 at boot
        case 0x1c: printf("cdvd: blue_led_ctl\n"); cdvd_s_blue_led_ctl(cdvd); break;
        case 0x1e: printf("cdvd: remote2_read\n"); cdvd_s_remote2_read(cdvd); break;
        case 0x22: printf("cdvd: read_wakeup_time\n"); cdvd_s_read_wakeup_time(cdvd); break;
        case 0x24: printf("cdvd: rc_bypass_ctrl\n"); cdvd_s_rc_bypass_ctrl(cdvd); break;
        case 0x29: printf("cdvd: notice_game_start\n"); cdvd_s_notice_game_start(cdvd); break;
        case 0x32: printf("cdvd: get_medium_removal\n"); cdvd_s_get_medium_removal(cdvd); break; 
        case 0x36: printf("cdvd: get_region_params\n"); cdvd_s_get_region_params(cdvd); break;
        case 0x40: printf("cdvd: open_config\n"); cdvd_s_open_config(cdvd); break;
        case 0x41: printf("cdvd: read_config\n"); cdvd_s_read_config(cdvd); break;
        case 0x42: printf("cdvd: write_config\n"); cdvd_s_write_config(cdvd); break;
        case 0x43: printf("cdvd: close_config\n"); cdvd_s_close_config(cdvd); break;
        case 0x80: printf("cdvd: mechacon_auth_80\n"); cdvd_s_mechacon_auth_80(cdvd); break;
        case 0x81: printf("cdvd: mechacon_auth_81\n"); cdvd_s_mechacon_auth_81(cdvd); break;
        case 0x82: printf("cdvd: mechacon_auth_82\n"); cdvd_s_mechacon_auth_82(cdvd); break;
        case 0x83: printf("cdvd: mechacon_auth_83\n"); cdvd_s_mechacon_auth_83(cdvd); break;
        case 0x84: printf("cdvd: mechacon_auth_84\n"); cdvd_s_mechacon_auth_84(cdvd); break;
        case 0x85: printf("cdvd: mechacon_auth_85\n"); cdvd_s_mechacon_auth_85(cdvd); break;
        case 0x86: printf("cdvd: mechacon_auth_86\n"); cdvd_s_mechacon_auth_86(cdvd); break;
        case 0x87: printf("cdvd: mechacon_auth_87\n"); cdvd_s_mechacon_auth_87(cdvd); break;
        case 0x88: printf("cdvd: mechacon_auth_88\n"); cdvd_s_mechacon_auth_88(cdvd); break;
        case 0x8d: printf("cdvd: mg_write_data\n"); cdvd_s_mg_write_data(cdvd); break;
        case 0x8f: printf("cdvd: mechacon_auth_8f\n"); cdvd_s_mechacon_auth_8f(cdvd); break;
        case 0x90: printf("cdvd: mg_write_hdr_start\n"); cdvd_s_mg_write_hdr_start(cdvd); break;
        case 0x91: printf("cdvd: mg_read_bit_length\n"); cdvd_s_mg_read_bit_length(cdvd); break;

        default: {
            fprintf(stderr, "cdvd: Unknown S command %02xh\n", cmd);

            exit(1);
        } break;
    }

    cdvd->s_param_index = 0;
}

static inline void cdvd_handle_s_param(struct ps2_cdvd* cdvd, uint8_t param) {
    if (cdvd->s_param_index == 16) {
        printf("cdvd: S parameter FIFO overflow\n");
    }

    cdvd->s_params[cdvd->s_param_index++] = param;
}

static inline uint8_t cdvd_read_s_response(struct ps2_cdvd* cdvd) {
    if (cdvd->s_fifo_index == cdvd->s_fifo_size) {
        return 0;
    }

    uint8_t data = cdvd->s_fifo[cdvd->s_fifo_index++];

    if (cdvd->s_fifo_index == cdvd->s_fifo_size)
        cdvd->s_stat |= 0x40;

    return data;
}

static inline long cdvd_get_cd_read_timing(struct ps2_cdvd* cdvd, int from) {
    long block_timing = (36864000L * cdvd->read_size) / (24 * 153600);
    long delta = cdvd->read_lba - from;
    long contiguous_cycles = block_timing * cdvd->read_count;

    if (!delta)
        return 1000;

    if (delta < 0) {
        delta = -delta;
    }

    // Small delta
    if (delta < 8)
        return ((block_timing * delta) + contiguous_cycles) / 4;

    // Fast seek: ~30ms
    if (delta < 4371)
        return (((36864000 / 1000) * 30) + contiguous_cycles) / 4;

    // Full seek: ~100ms
    return (((36864000 / 1000) * 100) + contiguous_cycles) / 4;
}

static inline void cdvd_set_status(struct ps2_cdvd* cdvd, uint8_t data) {
    cdvd->status = data;
    cdvd->sticky_status |= data;
}

static inline void cdvd_send_irq(struct ps2_cdvd* cdvd) {
    ps2_iop_intc_irq(cdvd->intc, IOP_INTC_CDVD);

    cdvd->i_stat |= 2;
}

void cdvd_fetch_sector(struct ps2_cdvd* cdvd) {
    memset(cdvd->buf, 0, 2352);

    switch (cdvd->read_size) {
        case CDVD_CD_SS_2048:
        case CDVD_CD_SS_2328: {
            disc_read_sector(cdvd->disc, cdvd->buf, cdvd->read_lba++, DISC_SS_DATA);
        } break;
        case CDVD_CD_SS_2352: {
            disc_read_sector(cdvd->disc, cdvd->buf, cdvd->read_lba++, DISC_SS_RAW);
        } break;
        case CDVD_CD_SS_2340: {
            // LBA -> MSF
            uint64_t a = cdvd->read_lba + 150;
            uint32_t m = a / 4500;

            a -= m * 4500;

            uint32_t s = a / 75;
            uint32_t f = a - (s * 75);

            // Fill in header
            cdvd->buf[0] = itob_table[m];
            cdvd->buf[1] = itob_table[s];
            cdvd->buf[2] = itob_table[f];
            cdvd->buf[3] = 1;

            // Write raw data at offset 12
            disc_read_sector(cdvd->disc, cdvd->buf + 12, cdvd->read_lba++, DISC_SS_DATA);
        } break;
        case CDVD_DVD_SS: {
            memset(cdvd->buf, 0, 2340);

            uint32_t lba, layer;

            if (cdvd->layer2_lba && (cdvd->read_lba >= cdvd->layer2_lba)) {
                layer = cdvd->read_lba >= cdvd->layer2_lba;
                lba = cdvd->read_lba - cdvd->layer2_lba + 0x30000;
            } else {
                layer = 0;
                lba = cdvd->read_lba + 0x30000;
            }

            cdvd->buf[0] = 0x20 | layer;
            cdvd->buf[1] = (lba >> 16) & 0xFF;
            cdvd->buf[2] = (lba >> 8) & 0xFF;
            cdvd->buf[3] = lba & 0xff;

            disc_read_sector(cdvd->disc, cdvd->buf + 12, cdvd->read_lba++, DISC_SS_DATA);

            // for (int i = 0; i < 2064;) {
            //     for (int x = 0; x < 16; x++) {
            //         printf("%02x ", cdvd->buf[i+x]);
            //     }
    
            //     putchar('|');
    
            //     for (int x = 0; x < 16; x++) {
            //         printf("%c", isprint(cdvd->buf[i+x]) ? cdvd->buf[i+x] : '.');
            //     }
    
            //     puts("|");
    
            //     i += 16;
            // }
        } break;
    }

    if (!cdvd->mecha_decode)
        return;

    uint8_t shift_amount = (cdvd->mecha_decode >> 4) & 7;
    int do_xor = (cdvd->mecha_decode) & 1;
    int do_shift = (cdvd->mecha_decode) & 2;

    for (int i = 0; i < cdvd->read_size; ++i) {
        if (do_xor) cdvd->buf[i] ^= cdvd->cdkey[4];
        if (do_shift) cdvd->buf[i] = (cdvd->buf[i] >> shift_amount) | (cdvd->buf[i] << (8 - shift_amount));
    }
}

void cdvd_do_read(void* udata, int overshoot) {
    struct ps2_cdvd* cdvd = (struct ps2_cdvd*)udata;

    // Ugly hack!!
    // Some games will send
    if (!(cdvd->dma->cdvd.chcr & 0x1000000)) {
        // printf("cdvd: CDVD DMA not yet ready\n");

        struct sched_event event;

        event.name = "CDVD Read";
        event.udata = cdvd;
        event.callback = cdvd_do_read;
        event.cycles = 1000;

        sched_schedule(cdvd->sched, event);

        cdvd_set_status(cdvd, CDVD_STATUS_READING);

        return;
    }

    // Fetch a sector
    cdvd_fetch_sector(cdvd);

    // Send sector to DMA
    cdvd->buf_size = cdvd->read_size;
    cdvd->read_count--;

    // printf("cdvd: Sending a sector to DMA (left=%d)\n", cdvd->read_count);

    iop_dma_handle_cdvd_transfer(cdvd->dma);

    if (cdvd->read_count) {
        struct sched_event event;

        event.name = "CDVD Read";
        event.udata = cdvd;
        event.callback = cdvd_do_read;
        event.cycles = 1000;

        sched_schedule(cdvd->sched, event);

        cdvd_set_status(cdvd, CDVD_STATUS_READING);

        return;
    }

    cdvd->n_stat = 0x4e;
    cdvd->n_cmd = 0;

    cdvd_set_ready(cdvd);
    cdvd_set_status(cdvd, CDVD_STATUS_PAUSED);

    cdvd_send_irq(cdvd);

    // I_STAT needs to be set to 3?
    cdvd->i_stat |= 2;
}

static inline void cdvd_n_nop(struct ps2_cdvd* cdvd) {
    printf("cdvd: nop\n");

    cdvd_set_ready(cdvd);
    cdvd_send_irq(cdvd);
}
static inline void cdvd_n_nop_sync(struct ps2_cdvd* cdvd) {
    fprintf(stderr, "cdvd: nop_sync\n"); exit(1);
}
static inline void cdvd_n_standby(struct ps2_cdvd* cdvd) {
    printf("cdvd: standby\n");

    cdvd_set_ready(cdvd);
    cdvd_set_status(cdvd, CDVD_STATUS_PAUSED);

    cdvd_send_irq(cdvd);
}
static inline void cdvd_n_stop(struct ps2_cdvd* cdvd) {
    printf("cdvd: stop\n");

    cdvd_set_ready(cdvd);
    cdvd_set_status(cdvd, CDVD_STATUS_STOPPED);

    cdvd_send_irq(cdvd);
}
static inline void cdvd_n_pause(struct ps2_cdvd* cdvd) {
    printf("cdvd: pause\n");

    cdvd_set_ready(cdvd);
    cdvd_set_status(cdvd, CDVD_STATUS_PAUSED);

    cdvd_send_irq(cdvd);

    cdvd->n_cmd = 0;
}
static inline void cdvd_n_seek(struct ps2_cdvd* cdvd) {
    printf("cdvd: seek\n");

    cdvd->read_lba = *(uint32_t*)(cdvd->n_params);

    cdvd_set_ready(cdvd);
    cdvd_set_status(cdvd, CDVD_STATUS_SEEKING);

    cdvd_send_irq(cdvd);
}
static inline void cdvd_n_read_cd(struct ps2_cdvd* cdvd) {
    /*  Params:
        0-3   Sector position
        4-7   Sectors to read
        10    Block size (1=2328 bytes, 2=2340 bytes, all others=2048 bytes)

            Performs a CD-style read. Seems to raise bit 0 of CDVD I_STAT upon completion?
    */

    int prev_lba = cdvd->read_lba;

    cdvd->read_lba = *(uint32_t*)(cdvd->n_params);
    cdvd->read_count = *(uint32_t*)(cdvd->n_params + 4);
    cdvd->read_speed = cdvd->n_params[9];
    cdvd->read_size = CDVD_CD_SS_2048;

    switch (cdvd->n_params[10]) {
        case 1: cdvd->read_size = CDVD_CD_SS_2328; break;
        case 2: cdvd->read_size = CDVD_CD_SS_2340; break;
    }

    struct sched_event event;

    event.name = "CDVD ReadCd";
    event.udata = cdvd;
    event.callback = cdvd_do_read;
    event.cycles = cdvd_get_cd_read_timing(cdvd, prev_lba);

    sched_schedule(cdvd->sched, event);

    cdvd_set_status(cdvd, CDVD_STATUS_READING);

    // printf("cdvd: ReadCd lba=%08x count=%08x size=%d cycles=%ld speed=%02x (%p)\n",
    //     cdvd->read_lba,
    //     cdvd->read_count,
    //     cdvd->read_size,
    //     event.cycles,
    //     cdvd->n_params[9],
    //     cdvd->disc->read_sector
    // );
}
static inline void cdvd_n_read_cdda(struct ps2_cdvd* cdvd) {
    int prev_lba = cdvd->read_lba;

    cdvd->read_lba = *(uint32_t*)(cdvd->n_params);
    cdvd->read_count = *(uint32_t*)(cdvd->n_params + 4);
    cdvd->read_speed = cdvd->n_params[9];
    cdvd->read_size = CDVD_CD_SS_2352;

    // fprintf(stderr, "cdvd: ReadCdda lba=%d count=%d decode=%d\n", cdvd->read_lba, cdvd->read_count, cdvd->mecha_decode);

    struct sched_event event;

    event.name = "CDVD ReadCdda";
    event.udata = cdvd;
    event.callback = cdvd_do_read;
    event.cycles = ((2352.f / 2.f) / 44100.f) * (36864000.f * 8);

    cdvd_set_status(cdvd, CDVD_STATUS_READING);

    sched_schedule(cdvd->sched, event);
}
static inline void cdvd_n_read_dvd(struct ps2_cdvd* cdvd) {
    /*  Params:
        0-3   Sector position
        4-7   Sectors to read

        Performs a DVD-style read, with a block size of 2064 bytes. The format of the data is as follows:
        0    1    Volume number + 0x20
        1    3    Sector number - volume start + 0x30000, in big-endian.
        4    8    ? (all zeroes)
        12   2048 Raw sector data
        2060 4    ? (all zeroes)
    */

    int prev_lba = cdvd->read_lba;

    cdvd->read_lba = *(uint32_t*)(cdvd->n_params);
    cdvd->read_count = *(uint32_t*)(cdvd->n_params + 4);
    cdvd->read_speed = cdvd->n_params[9];
    cdvd->read_size = CDVD_DVD_SS;

    struct sched_event event;

    event.name = "CDVD ReadDvd";
    event.udata = cdvd;
    event.callback = cdvd_do_read;
    event.cycles = cdvd_get_cd_read_timing(cdvd, prev_lba);

    sched_schedule(cdvd->sched, event);

    cdvd_set_status(cdvd, CDVD_STATUS_READING);
}
static inline void cdvd_n_get_toc(struct ps2_cdvd* cdvd) {
    fprintf(stderr, "cdvd: get_toc\n");

    memset(cdvd->buf, 0, 2064);

    if (cdvd->disc_type == CDVD_DISC_CDDA) {
        int track_count = disc_get_track_count(cdvd->disc);
        int disc_size = disc_get_size(cdvd->disc) / 2352;

        int size_mm = disc_size / (60 * 75);
        int size_ss = (disc_size % (60 * 75)) / 75;
        int size_ff = (disc_size % (60 * 75)) % 75;

        cdvd->buf[0] = 0x41;
		cdvd->buf[1] = 0x00;

		//Number of FirstTrack
		cdvd->buf[2] = 0xa0;
		cdvd->buf[7] = 0x01;

		//Number of LastTrack
		cdvd->buf[12] = 0xa1;
		cdvd->buf[17] = itob_table[track_count];

		//DiskLength
		cdvd->buf[22] = 0xa2;
		cdvd->buf[27] = itob_table[size_mm]; // mm
		cdvd->buf[28] = itob_table[size_ss]; // ss
		cdvd->buf[29] = itob_table[size_ff]; // ff

        for (int i = 0; i < track_count; i++) {
            int num = i + 1;

            struct track_info info;

            disc_get_track_info(cdvd->disc, num, &info);

            int track_mm = info.lba / (60 * 75);
            int track_ss = (info.lba % (60 * 75)) / 75;
            int track_ff = (info.lba % (60 * 75)) % 75;

            // fprintf(stderr, "cdvd: track %d %02d:%02d:%02d\n", i + 1, track_mm, track_ss, track_ff);

            cdvd->buf[30 + (i * 10)] = info.type;
            cdvd->buf[32 + (i * 10)] = itob_table[num]; // Track number
            cdvd->buf[37 + (i * 10)] = itob_table[track_mm]; // mm
            cdvd->buf[38 + (i * 10)] = itob_table[track_ss]; // ss
            cdvd->buf[39 + (i * 10)] = itob_table[track_ff]; // ff
        }
    } else if (!cdvd_is_dual_layer(cdvd)) {
        cdvd->buf[0] = 0x04;
        cdvd->buf[1] = 0x02;
        cdvd->buf[2] = 0xF2;
        cdvd->buf[3] = 0x00;
        cdvd->buf[4] = 0x86;
        cdvd->buf[5] = 0x72;
        cdvd->buf[17] = 0x03;
    } else {
        cdvd->buf[0] = 0x24;
        cdvd->buf[1] = 0x02;
        cdvd->buf[2] = 0xF2;
        cdvd->buf[3] = 0x00;
        cdvd->buf[4] = 0x41;
        cdvd->buf[5] = 0x95;

        cdvd->buf[14] = 0x60;

        cdvd->buf[16] = 0x00;
        cdvd->buf[17] = 0x03;
        cdvd->buf[18] = 0x00;
        cdvd->buf[19] = 0x00;

        int32_t start = cdvd->layer2_lba + 0x30000 - 1;

        cdvd->buf[20] = start >> 24;
        cdvd->buf[21] = (start >> 16) & 0xff;
        cdvd->buf[22] = (start >> 8) & 0xff;
        cdvd->buf[23] = start & 0xFF;
    }

    cdvd->buf_size = 2064;
    cdvd->n_stat = 0x40;

    iop_dma_handle_cdvd_transfer(cdvd->dma);

    cdvd_set_ready(cdvd);
    cdvd_set_status(cdvd, CDVD_STATUS_READING);

    cdvd_send_irq(cdvd);

    cdvd->n_cmd = 0;
}
static inline void cdvd_n_read_key(struct ps2_cdvd* cdvd) {
    uint32_t b0 = cdvd->n_params[3];
    uint32_t b1 = cdvd->n_params[4];
    uint32_t b2 = cdvd->n_params[5];
    uint32_t b3 = cdvd->n_params[6];
    uint32_t arg = b0 | (b1 << 8) | (b2 << 16) | (b3 << 24);

    // Code referenced/taken from PCSX2
    // This performs some kind of encryption/checksum with the game's serial?
    memset(cdvd->cdkey, 0, 16);

    char serial[16];

    if (!disc_get_serial(cdvd->disc, serial)) {
        printf("cdvd: Couldn't find game serial, can't get cdkey\n");
    } else {
        printf("cdvd: \'%s\'\n", serial);
    }

    int32_t letters = (int32_t)((serial[3] & 0x7F) << 0) |
                (int32_t)((serial[2] & 0x7F) << 7) |
                (int32_t)((serial[1] & 0x7F) << 14) |
                (int32_t)((serial[0] & 0x7F) << 21);

    char m[6];

    for (int i = 0; i < 3; i++)
        m[i] = serial[i+5];

    for (int i = 0; i < 2; i++)
        m[i+3] = serial[i+9];

    m[5] = '\0';
    
    int32_t code = strtoul(m, NULL, 10);

    uint32_t key_0_3 = ((code & 0x1FC00) >> 10) | ((0x01FFFFFF & letters) << 7);
    uint32_t key_4 = ((code & 0x0001F) << 3) | ((0x0E000000 & letters) >> 25);
    uint32_t key_14 = ((code & 0x003E0) >> 2) | 0x04;

    cdvd->cdkey[0] = (key_0_3 & 0x000000FF) >> 0;
    cdvd->cdkey[1] = (key_0_3 & 0x0000FF00) >> 8;
    cdvd->cdkey[2] = (key_0_3 & 0x00FF0000) >> 16;
    cdvd->cdkey[3] = (key_0_3 & 0xFF000000) >> 24;
    cdvd->cdkey[4] = key_4;

    switch (arg) {
        case 75: {
            cdvd->cdkey[14] = key_14;
            cdvd->cdkey[15] = 0x05;
        } break;
        case 4246: {
            cdvd->cdkey[0] = 0x07;
            cdvd->cdkey[1] = 0xF7;
            cdvd->cdkey[2] = 0xF2;
            cdvd->cdkey[3] = 0x01;
            cdvd->cdkey[4] = 0x00;
            cdvd->cdkey[15] = 0x01;
        } break;
        default: {
            cdvd->cdkey[15] = 0x01;
        } break;
    }

    cdvd_set_ready(cdvd);
    cdvd_send_irq(cdvd);
}

static inline void cdvd_handle_n_command(struct ps2_cdvd* cdvd, uint8_t cmd) {
    cdvd->n_cmd = cmd;

    // fprintf(stdout, "cdvd: N command %s (%02x)\n", cdvd_get_n_command_name(cmd), cmd);

    cdvd_set_busy(cdvd);

    switch (cdvd->n_cmd) {
        case 0x00: cdvd_n_nop(cdvd); break;
        case 0x01: cdvd_n_nop_sync(cdvd); break;
        case 0x02: cdvd_n_standby(cdvd); break;
        case 0x03: cdvd_n_stop(cdvd); break;
        case 0x04: cdvd_n_pause(cdvd); break;
        case 0x05: cdvd_n_seek(cdvd); break;
        case 0x06: cdvd_n_read_cd(cdvd); break;
        case 0x07: cdvd_n_read_cdda(cdvd); break;
        case 0x08: cdvd_n_read_dvd(cdvd); break;
        case 0x09: cdvd_n_get_toc(cdvd); break;
        case 0x0c: cdvd_n_read_key(cdvd); break;
        default: {
            fprintf(stderr, "cdvd: Unhandled N command %02x\n", cdvd->n_cmd);

            exit(1);
        } break;
    }

    // Reset N param FIFO
    cdvd->n_param_index = 0;
}

static inline void cdvd_handle_n_param(struct ps2_cdvd* cdvd, uint8_t param) {
    cdvd->n_params[cdvd->n_param_index++] = param;

    if (cdvd->n_param_index > 15) {
        fprintf(stderr, "cdvd: N parameter FIFO overflow\n");

        exit(1);
    }
}

struct ps2_cdvd* ps2_cdvd_create(void) {
    return malloc(sizeof(struct ps2_cdvd));
}

void ps2_cdvd_init(struct ps2_cdvd* cdvd, struct ps2_iop_dma* dma, struct ps2_iop_intc* intc, struct sched_state* sched) {
    memset(cdvd, 0, sizeof(struct ps2_cdvd));

    // 00:02:00
    cdvd->read_lba = 0x150;

    cdvd->n_stat = 0x4e;
    cdvd->s_stat = CDVD_S_STATUS_NO_DATA;
    cdvd->sticky_status = 0x1e;
    cdvd->sched = sched;
    cdvd->dma = dma;
    cdvd->intc = intc;

    memcpy(cdvd->nvram, nvram_init_data, 1024);

    FILE* file = fopen("nvram.bin", "rb");

    if (!file)
        return;

    fread(cdvd->nvram, 1, 1024, file);
    fclose(file);
}

void ps2_cdvd_destroy(struct ps2_cdvd* cdvd) {
    ps2_cdvd_close(cdvd);

    free(cdvd->s_fifo);
    free(cdvd);
}

void cdvd_set_detected_type(void* udata, int overshoot) {
    struct ps2_cdvd* cdvd = (struct ps2_cdvd*)udata;

    cdvd_set_status(cdvd, CDVD_STATUS_PAUSED);

    cdvd->disc_type = cdvd->detected_disc_type;
}

int ps2_cdvd_open(struct ps2_cdvd* cdvd, const char* path, int delay) {
    ps2_cdvd_close(cdvd);

    cdvd->layer2_lba = 0;

    cdvd->disc = disc_open(path);

    if (!cdvd->disc) {
        printf("cdvd: Couldn't open disc \'%s\'\n", path);

        return 1;
    }

    cdvd->detected_disc_type = disc_get_type(cdvd->disc);
    cdvd->layer2_lba = disc_get_volume_lba(cdvd->disc, 1);

    fprintf(stdout, "cdvd: Opened \'%s\' (%s)\n", path, cdvd_get_type_name(cdvd->detected_disc_type));

    if (!delay) {
        cdvd_set_status(cdvd, CDVD_STATUS_SPINNING);

        cdvd->disc_type = cdvd->detected_disc_type;

        return 0;
    }

    switch (cdvd->detected_disc_type) {
        case CDVD_DISC_PS2_CD:
        case CDVD_DISC_PS2_CDDA:
        case CDVD_DISC_CDDA:
        case CDVD_DISC_PSX_CD:
        case CDVD_DISC_PSX_CDDA: {
            cdvd->disc_type = CDVD_DISC_DETECTING_CD;
        } break;

        case CDVD_DISC_PS2_DVD:
        case CDVD_DISC_DVD_VIDEO: {
            cdvd->disc_type = cdvd->layer2_lba ? CDVD_DISC_DETECTING_DL_DVD : CDVD_DISC_DETECTING_DVD;
        } break;
    }

    cdvd_set_status(cdvd, CDVD_STATUS_TRAY_OPEN_BIT);

    struct sched_event event;

    event.cycles = delay; // IOP clock * 2 = 2s
    event.udata = cdvd;
    event.name = "CDVD disc detect";
    event.callback = cdvd_set_detected_type;

    sched_schedule(cdvd->sched, event);

    return 0;
}

void ps2_cdvd_close(struct ps2_cdvd* cdvd) {
    if (cdvd->disc) {
        disc_close(cdvd->disc);

        cdvd->disc = NULL;
    }

    cdvd->disc_type = CDVD_DISC_NO_DISC;

    cdvd_set_status(cdvd, CDVD_STATUS_TRAY_OPEN_BIT);

    // Send disc ejected IRQ
    cdvd->i_stat = CDVD_IRQ_DISC_EJECTED;

    ps2_iop_intc_irq(cdvd->intc, IOP_INTC_CDVD);
}

void ps2_cdvd_power_off(struct ps2_cdvd* cdvd) {
    // Send poweroff IRQ
    cdvd->i_stat = CDVD_IRQ_POWER_OFF;

    ps2_iop_intc_irq(cdvd->intc, IOP_INTC_CDVD);
}

uint8_t cdvd_read_speed(struct ps2_cdvd* cdvd) {
    uint8_t speed = cdvd->read_speed & 0x3F;

    if (!speed)
        speed = (cdvd->disc_type == CDVD_DISC_PS2_DVD) ? 3 : 5;

    if (cdvd->disc_type == CDVD_DISC_PS2_DVD)
        speed += 0xF;
    else
        speed--;

    return speed;
}

uint64_t ps2_cdvd_read8(struct ps2_cdvd* cdvd, uint32_t addr) {
    // printf("cdvd: read %08x\n", addr);

    switch (addr) {
        case 0x1F402004: printf("cdvd: read n_cmd %x\n", cdvd->n_cmd); return cdvd->n_cmd;
        case 0x1F402005: printf("cdvd: read n_stat %x\n", cdvd->n_stat); return cdvd->n_stat;
        // case 0x1F402005: (W)
        case 0x1F402006: printf("cdvd: read error %x\n", 0); return 0; //cdvd->error;
        // case 0x1F402007: (W)
        case 0x1F402008: printf("cdvd: read i_stat %x\n", cdvd->i_stat); return cdvd->i_stat;
        case 0x1F40200A: printf("cdvd: read status %x\n", cdvd->status); return cdvd->status;
        case 0x1F40200B: printf("cdvd: read sticky_status %x\n", cdvd->sticky_status); return cdvd->sticky_status;
        case 0x1F40200F: printf("cdvd: read disc_type %x\n", cdvd->disc_type); return cdvd->disc_type;
        case 0x1F402013: printf("cdvd: read speed %x\n", cdvd_read_speed(cdvd)); return cdvd_read_speed(cdvd);
        case 0x1F402015: return 0xff;
        case 0x1F402016: printf("cdvd: read s_cmd %x\n", cdvd->s_cmd); return cdvd->s_cmd;
        case 0x1F402017: printf("cdvd: read s_stat %x\n", cdvd->s_stat); return cdvd->s_stat;
        // case 0x1F402017: (W);
        case 0x1F402018: { int r = cdvd_read_s_response(cdvd); printf("cdvd: read s_response %x\n", r); return r; }

        case 0x1F402020:
        case 0x1F402021:
        case 0x1F402022:
        case 0x1F402023:
        case 0x1F402024:
            printf("cdvd: ReadKey %08x (%d) -> %02x\n", addr, addr - 0x1f402020, cdvd->cdkey[addr - 0x1F402020]);
            return cdvd->cdkey[addr - 0x1F402020];
        case 0x1F402028:
        case 0x1F402029:
        case 0x1F40202A:
        case 0x1F40202B:
        case 0x1F40202C:
            printf("cdvd: ReadKey %08x (%d) -> %02x\n", addr, addr - 0x1f402023, cdvd->cdkey[addr - 0x1F402023]);
            return cdvd->cdkey[addr - 0x1F402023];
        case 0x1F402030:
        case 0x1F402031:
        case 0x1F402032:
        case 0x1F402033:
        case 0x1F402034:
            printf("cdvd: ReadKey %08x (%d) -> %02x\n", addr, addr - 0x1f402026, cdvd->cdkey[addr - 0x1F402026]);
            return cdvd->cdkey[addr - 0x1F402026];
        case 0x1F402038:
            printf("cdvd: ReadKey %08x (%d) -> %02x\n", addr, addr - 0x1f402038, cdvd->cdkey[15]);
            return cdvd->cdkey[15];
    }

    printf("cdvd: unknown read %08x\n", addr);
    
    return 0;
}

void ps2_cdvd_write8(struct ps2_cdvd* cdvd, uint32_t addr, uint64_t data) {
    // printf("cdvd: write %08x %02x\n", addr, data);

    switch (addr) {
        case 0x1F402004: cdvd_handle_n_command(cdvd, data); return;
        case 0x1F402005: cdvd_handle_n_param(cdvd, data); return;
        case 0x1F402006: /* Read-only */ return;
        case 0x1F402007: printf("cdvd: break\n"); /* To-do: BREAK */ return;
        case 0x1F402008: cdvd->i_stat &= ~data; return;
        case 0x1F40200A: return;
        case 0x1F40200B: return;
        case 0x1F40200F: return;
        case 0x1F402016: cdvd_handle_s_command(cdvd, data); return;
        case 0x1F402017: cdvd_handle_s_param(cdvd, data); return;
        // case 0x1F402017: (W);
        case 0x1F40203A: cdvd->mecha_decode = data; return;
        case 0x1F402018: return;
    }

    return;
}

void ps2_cdvd_reset(struct ps2_cdvd* cdvd) {
    cdvd->n_stat = 0x4c;
    cdvd->read_lba = 0x150;
    cdvd->read_count = 0;
    cdvd->read_size = 0;
    cdvd->read_speed = 0;

    cdvd->config_rw = 0;
    cdvd->config_offset = 0;
    cdvd->config_numblocks = 0;
    cdvd->config_block_index = 0;
    cdvd->s_cmd = 0;
    cdvd->buf_size = 0;
}

void ps2_cdvd_set_mechacon_model(struct ps2_cdvd* cdvd, int model) {
    cdvd->mechacon_model = model;

    switch (model) {
        case CDVD_MECHACON_SPC970: {
            cdvd->layout = g_spc970_layout;
        } break;

        case CDVD_MECHACON_DRAGON: {
            cdvd->layout = g_dragon_layout;
        } break;
    }
}