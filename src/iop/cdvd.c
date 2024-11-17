#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "cdvd.h"

static inline void cdvd_init_s_fifo(struct ps2_cdvd* cdvd, int size) {
    if (cdvd->s_fifo)
        free(cdvd->s_fifo);

    cdvd->s_fifo_size = size;
    cdvd->s_fifo_index = 0;
    cdvd->s_fifo = malloc(cdvd->s_fifo_size);
    cdvd->s_stat &= ~0x40;
}

static inline void cdvd_s_mechacon_version(struct ps2_cdvd* cdvd) {}
static inline void cdvd_s_update_sticky_flags(struct ps2_cdvd* cdvd) {}
static inline void cdvd_s_read_rtc(struct ps2_cdvd* cdvd) {
    cdvd_init_s_fifo(cdvd, 8);

    cdvd->s_fifo[0] = 0;
    cdvd->s_fifo[1] = 1;
    cdvd->s_fifo[2] = 2;
    cdvd->s_fifo[3] = 3;
    cdvd->s_fifo[4] = 0;
    cdvd->s_fifo[5] = 5;
    cdvd->s_fifo[6] = 6;
    cdvd->s_fifo[7] = 7;
}
static inline void cdvd_s_write_rtc(struct ps2_cdvd* cdvd) {}
static inline void cdvd_s_forbid_dvd(struct ps2_cdvd* cdvd) {}
static inline void cdvd_s_open_config(struct ps2_cdvd* cdvd) {}
static inline void cdvd_s_read_config(struct ps2_cdvd* cdvd) {}
static inline void cdvd_s_close_config(struct ps2_cdvd* cdvd) {}

void cdvd_handle_s_command(struct ps2_cdvd* cdvd, uint8_t cmd) {
    // Handle MechaconVersion command
    if (cdvd->s_cmd == 3) {
        if (cmd != 0) {
            printf("cdvd: Unknown S command 03h:%02xh\n", cmd);

            exit(1);
        }

        cdvd_s_mechacon_version(cdvd);

        cdvd->s_cmd = 0;
        cdvd->s_param_index = 0;

        return;
    }

    cdvd->s_cmd = cmd;

    switch (cmd) {
        // 03h: Subcommand
        case 0x03: return;
        case 0x05: cdvd_s_update_sticky_flags(cdvd); break;
        case 0x08: cdvd_s_read_rtc(cdvd); break;
        case 0x09: cdvd_s_write_rtc(cdvd); break;
        case 0x15: cdvd_s_forbid_dvd(cdvd); break;
        case 0x40: cdvd_s_open_config(cdvd); break;
        case 0x41: cdvd_s_read_config(cdvd); break;
        case 0x43: cdvd_s_close_config(cdvd); break;

        default: {
            printf("cdvd: Unknown S command %02xh\n", cmd);

            exit(1);
        } break;
    }

    cdvd->s_cmd = 0;
    cdvd->s_param_index = 0;
}

static inline void cdvd_handle_s_param(struct ps2_cdvd* cdvd, uint8_t param) {
    cdvd->s_params[cdvd->s_param_index++] = param;

    if (cdvd->s_param_index > 15) {
        printf("cdvd: S parameter FIFO overflow\n");

        exit(1);
    }
}

static inline uint8_t cdvd_read_s_response(struct ps2_cdvd* cdvd) {
    if (cdvd->s_fifo_index == cdvd->s_fifo_size) {
        printf("cdvd: S response FIFO overflow\n");

        exit(1);
    }

    uint8_t data = cdvd->s_fifo[cdvd->s_fifo_index++];

    if (cdvd->s_fifo_index == cdvd->s_fifo_size)
        cdvd->s_stat |= 0x40;

    return data;
}

struct ps2_cdvd* ps2_cdvd_create(void) {
    return malloc(sizeof(struct ps2_cdvd));
}

void ps2_cdvd_init(struct ps2_cdvd* cdvd) {
    memset(cdvd, 0, sizeof(struct ps2_cdvd));

    cdvd->n_stat = 0x40;
    cdvd->s_stat = 0x40;
}

void ps2_cdvd_destroy(struct ps2_cdvd* cdvd) {
    free(cdvd->s_fifo);
    free(cdvd);
}

// To-do: Disc images
void ps2_cdvd_open(struct ps2_cdvd* cdvd, const char* path);
void ps2_cdvd_close(struct ps2_cdvd* cdvd);

uint64_t ps2_cdvd_read8(struct ps2_cdvd* cdvd, uint32_t addr) {
    switch (addr) {
        case 0x1F402004: return cdvd->n_cmd;
        case 0x1F402005: return cdvd->n_stat;
        // case 0x1F402005: (W)
        case 0x1F402006: return cdvd->error;
        // case 0x1F402007: (W)
        case 0x1F402008: return cdvd->i_stat;
        case 0x1F40200A: return cdvd->status;
        case 0x1F40200F: return cdvd->disc_type;
        case 0x1F402016: return cdvd->s_cmd;
        case 0x1F402017: return cdvd->s_stat;
        // case 0x1F402017: (W);
        case 0x1F402018: return cdvd_read_s_response(cdvd);
    }
    
    return 0;
}

void ps2_cdvd_write8(struct ps2_cdvd* cdvd, uint32_t addr, uint64_t data) {
    switch (addr) {
        case 0x1F402004: return;
        case 0x1F402005: return;
        // case 0x1F402005: (W)
        case 0x1F402006: return;
        // case 0x1F402007: (W)
        case 0x1F402008: cdvd->i_stat &= ~data; return;
        case 0x1F40200A: return;
        case 0x1F40200F: return;
        case 0x1F402016: cdvd_handle_s_command(cdvd, data); return;
        case 0x1F402017: cdvd_handle_s_param(cdvd, data); return;
        // case 0x1F402017: (W);
        case 0x1F402018: return;
    }

    return;
}