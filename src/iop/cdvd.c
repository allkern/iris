#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <time.h>

#include "cdvd.h"

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

static inline void cdvd_set_busy(struct ps2_cdvd* cdvd) {
    cdvd->n_stat |= CDVD_N_STATUS_BUSY;
    cdvd->n_stat &= ~CDVD_N_STATUS_READY;
}

static inline void cdvd_set_ready(struct ps2_cdvd* cdvd) {
    cdvd->n_stat &= ~CDVD_N_STATUS_BUSY;
    cdvd->n_stat |= CDVD_N_STATUS_READY;
}

void cdvd_read_sector(struct ps2_cdvd* cdvd, int lba, int offset) {
    // printf("cdvd: Read lba=%d (%x)\n", lba, lba);
    fseek(cdvd->file, lba * 0x800, SEEK_SET);
    fread(cdvd->buf + offset, 1, 0x800, cdvd->file);
}

static inline void cdvd_init_s_fifo(struct ps2_cdvd* cdvd, int size) {
    if (cdvd->s_fifo)
        free(cdvd->s_fifo);

    cdvd->s_fifo_size = size;
    cdvd->s_fifo_index = 0;
    cdvd->s_fifo = malloc(cdvd->s_fifo_size);
    cdvd->s_stat &= ~0x40;
}

static inline void cdvd_s_mechacon_version(struct ps2_cdvd* cdvd) {
    switch (cdvd->s_params[0]) {
        case 0x00: {
            cdvd_init_s_fifo(cdvd, 4);

            cdvd->s_fifo[0] = 0x03;
            cdvd->s_fifo[1] = 0x06;
            cdvd->s_fifo[2] = 0x02;
            cdvd->s_fifo[3] = 0x00;
        } break;

        case 0xef: {
            cdvd_init_s_fifo(cdvd, 3);

            cdvd->s_fifo[0] = 0x00;
            cdvd->s_fifo[1] = 0x0f;
            cdvd->s_fifo[2] = 0x05;
        } break;

        default: {
            printf("cdvd: Unknown S subcommand %02x\n", cdvd->s_params[0]);

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
    printf("cdvd: write_rtc\n");

    exit(1);
}
static inline void cdvd_s_read_nvram(struct ps2_cdvd* cdvd) {
    uint16_t addr = *(uint16_t*)&cdvd->s_params[0];

    cdvd_init_s_fifo(cdvd, 2);

    cdvd->s_fifo[0] = cdvd->nvram[(addr << 1) + 0];
    cdvd->s_fifo[1] = cdvd->nvram[(addr << 1) + 1];
}
static inline void cdvd_s_write_nvram(struct ps2_cdvd* cdvd) {
    printf("cdvd: write_rtc\n");

    exit(1);
}
static inline void cdvd_s_forbid_dvd(struct ps2_cdvd* cdvd) {
    cdvd_init_s_fifo(cdvd, 1);

    cdvd->s_fifo[0] = 5;
}
static inline void cdvd_s_read_ilink_model(struct ps2_cdvd* cdvd) {
    cdvd_init_s_fifo(cdvd, 9);

    for (int i = 0; i < 9; i++)
        cdvd->s_fifo[i] = 0;
}
static inline void cdvd_s_certify_boot(struct ps2_cdvd* cdvd) {
    cdvd_init_s_fifo(cdvd, 1);

    cdvd->s_fifo[0] = 1;
}
static inline void cdvd_s_cancel_pwoff_ready(struct ps2_cdvd* cdvd) {
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

    cdvd->s_fifo[0] = 0;
}
static inline void cdvd_s_read_config(struct ps2_cdvd* cdvd) {
    cdvd_init_s_fifo(cdvd, 16);

    for (int i = 0; i < 16; i++)
        cdvd->s_fifo[i] = 0;
}
static inline void cdvd_s_close_config(struct ps2_cdvd* cdvd) {
    cdvd_init_s_fifo(cdvd, 1);

    cdvd->s_fifo[0] = 0;
}

void cdvd_handle_s_command(struct ps2_cdvd* cdvd, uint8_t cmd) {
    cdvd->s_cmd = cmd;

    switch (cmd) {
        case 0x03: cdvd_s_mechacon_version(cdvd); break;
        case 0x05: cdvd_s_update_sticky_flags(cdvd); break;
        case 0x08: cdvd_s_read_rtc(cdvd); break;
        case 0x09: cdvd_s_write_rtc(cdvd); break;
        case 0x0a: cdvd_s_read_nvram(cdvd); break;
        case 0x0b: cdvd_s_write_nvram(cdvd); break;
        case 0x15: cdvd_s_forbid_dvd(cdvd); break;
        case 0x17: cdvd_s_read_ilink_model(cdvd); break;
        case 0x1a: cdvd_s_certify_boot(cdvd); break;
        case 0x1b: cdvd_s_cancel_pwoff_ready(cdvd); break;
        case 0x22: cdvd_s_read_wakeup_time(cdvd); break;
        case 0x24: cdvd_s_rc_bypass_ctrl(cdvd); break;
        case 0x40: cdvd_s_open_config(cdvd); break;
        case 0x41: cdvd_s_read_config(cdvd); break;
        case 0x43: cdvd_s_close_config(cdvd); break;

        default: {
            printf("cdvd: Unknown S command %02xh\n", cmd);

            exit(1);
        } break;
    }

    // printf("cdvd: S command %02x response: ", cmd);

    // for (int i = 0; i < cdvd->s_fifo_size; i++) {
    //     printf("%02x ", cdvd->s_fifo[i]);
    // }

    // putchar('\n');

    // cdvd->s_cmd = 0;
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
        return 0;

    if (delta < 0) {
        delta = -delta;
    }

    // Small delta
    if (delta < 8)
        return (block_timing * delta) + contiguous_cycles;

    // Fast seek: ~30ms
    if (delta < 4371)
        return ((36864000 / 1000) * 30) + contiguous_cycles;

    // Full seek: ~100ms
    return ((36864000 / 1000) * 100) + contiguous_cycles;
}

static inline void cdvd_set_status_bits(struct ps2_cdvd* cdvd, uint8_t data) {
    cdvd->status |= data;
    cdvd->sticky_status |= data;
}

void cdvd_do_cd_read(void* udata, int overshoot) {
    struct ps2_cdvd* cdvd = (struct ps2_cdvd*)udata;

    assert(cdvd->read_size == 2048);

    // printf("cdvd: Reading %d sector(s) from lba=%x (%d)\n", cdvd->read_count, cdvd->read_lba, cdvd->read_lba);

    // Do read
    int sector_count = cdvd->read_count;

    for (int i = 0; i < sector_count; i++) {
        memset(cdvd->buf, 0, 2340);

        // LBA -> MSF
        uint64_t a = (cdvd->read_lba + i) + 150;
        uint32_t m = a / 4500;

        a -= m * 4500;

        uint32_t s = a / 75;
        uint32_t f = a - (s * 75);

        // // Write sync data
        // for (int i = 0x1; i < 0xb; i++)
        //     buf[i] = 0xff;

        // Fill in header
        cdvd->buf[12] = itob_table[m];
        cdvd->buf[13] = itob_table[s];
        cdvd->buf[14] = itob_table[f];
        cdvd->buf[15] = 1;

        // Write raw data at offset 12
        cdvd_read_sector(cdvd, cdvd->read_lba++, 0);

        cdvd->buf_size = cdvd->read_size;
        cdvd->read_count--;

        iop_dma_handle_cdvd_transfer(cdvd->dma);
    }

    // Raise both bit 0 and 1 of I_STAT
    cdvd->i_stat |= 1;

    // Clear status
    cdvd->status &= ~(CDVD_STATUS_READING | CDVD_STATUS_SEEKING | CDVD_STATUS_SPINNING);

    cdvd_set_ready(cdvd);
    cdvd_set_status_bits(cdvd, CDVD_STATUS_PAUSED);

    // Send IRQ to IOP
    ps2_iop_intc_irq(cdvd->intc, IOP_INTC_CDVD);

    // printf("cdvd: Read done\n");
}

static inline void cdvd_n_nop(struct ps2_cdvd* cdvd) {
    printf("cdvd: nop\n"); exit(1);
}
static inline void cdvd_n_nop_sync(struct ps2_cdvd* cdvd) {
    printf("cdvd: nop_sync\n"); exit(1);
}
static inline void cdvd_n_standby(struct ps2_cdvd* cdvd) {
    printf("cdvd: standby\n"); exit(1);
}
static inline void cdvd_n_stop(struct ps2_cdvd* cdvd) {
    printf("cdvd: stop\n");

    cdvd_set_ready(cdvd);
    cdvd_set_status_bits(cdvd, CDVD_STATUS_PAUSED);

    cdvd->status &= ~(CDVD_STATUS_READING | CDVD_STATUS_SEEKING | CDVD_STATUS_SPINNING);

    // Send IRQ to IOP
    ps2_iop_intc_irq(cdvd->intc, IOP_INTC_CDVD);
}
static inline void cdvd_n_pause(struct ps2_cdvd* cdvd) {
    printf("cdvd: pause\n");

    cdvd_set_ready(cdvd);
    cdvd_set_status_bits(cdvd, CDVD_STATUS_PAUSED);

    cdvd->status &= ~(CDVD_STATUS_READING | CDVD_STATUS_SEEKING | CDVD_STATUS_SPINNING);

    // Send IRQ to IOP
    ps2_iop_intc_irq(cdvd->intc, IOP_INTC_CDVD);
}
static inline void cdvd_n_seek(struct ps2_cdvd* cdvd) {
    printf("cdvd: seek\n"); exit(1);
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
    cdvd->read_size = 2048;

    switch (cdvd->n_params[10]) {
        case 1: cdvd->read_size = 2328; break;
        case 2: cdvd->read_size = 2340; break;
    }

    struct sched_event event;

    event.name = "CDVD ReadCd";
    event.udata = cdvd;
    event.callback = cdvd_do_cd_read;
    event.cycles = cdvd_get_cd_read_timing(cdvd, prev_lba);

    cdvd_set_status_bits(cdvd,
        CDVD_STATUS_READING |
        CDVD_STATUS_SPINNING |
        CDVD_STATUS_SEEKING
    );

    // printf("cdvd: ReadCd lba=%08x count=%08x size=%d cycles=%ld\n",
    //     cdvd->read_lba,
    //     cdvd->read_count,
    //     cdvd->read_size,
    //     event.cycles
    // );

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

    uint32_t lba = *(uint32_t*)(cdvd->n_params);
    uint32_t count = *(uint32_t*)(cdvd->n_params + 4);

    printf("cdvd: ReadDvd lba=%08x count=%08x\n", lba, count);

    exit(1);

}
static inline void cdvd_n_get_toc(struct ps2_cdvd* cdvd) {
    printf("cdvd: get_toc\n");

    memset(cdvd->buf, 0, 2064);

    cdvd->buf[0] = 0x04;
    cdvd->buf[1] = 0x02;
    cdvd->buf[2] = 0xF2;
    cdvd->buf[3] = 0x00;
    cdvd->buf[4] = 0x86;
    cdvd->buf[5] = 0x72;
    cdvd->buf[17] = 0x03;

    cdvd->buf_size = 2064;

    iop_dma_handle_cdvd_transfer(cdvd->dma);

    cdvd_set_ready(cdvd);
    cdvd_set_status_bits(cdvd, CDVD_STATUS_PAUSED);

    // Send IRQ to IOP
    ps2_iop_intc_irq(cdvd->intc, IOP_INTC_CDVD);
}

static inline void cdvd_handle_n_command(struct ps2_cdvd* cdvd, uint8_t cmd) {
    cdvd->n_cmd = cmd;

    cdvd_set_busy(cdvd);

    switch (cdvd->n_cmd) {
        case 0x00: cdvd_n_nop(cdvd); break;
        case 0x01: cdvd_n_nop_sync(cdvd); break;
        case 0x02: cdvd_n_standby(cdvd); break;
        case 0x03: cdvd_n_stop(cdvd); break;
        case 0x04: cdvd_n_pause(cdvd); break;
        case 0x05: cdvd_n_seek(cdvd); break;
        case 0x06: cdvd_n_read_cd(cdvd); break;
        case 0x08: cdvd_n_read_dvd(cdvd); break;
        case 0x09: cdvd_n_get_toc(cdvd); break;
    }

    // Reset N param FIFO
    cdvd->n_param_index = 0;
}

static inline void cdvd_handle_n_param(struct ps2_cdvd* cdvd, uint8_t param) {
    cdvd->n_params[cdvd->n_param_index++] = param;

    if (cdvd->n_param_index > 15) {
        printf("cdvd: N parameter FIFO overflow\n");

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

    cdvd->n_stat = 0x4c;
    cdvd->s_stat = CDVD_S_STATUS_NO_DATA;
    cdvd->sticky_status = 0x1e;
    cdvd->sched = sched;
    cdvd->dma = dma;
    cdvd->intc = intc;
}

void ps2_cdvd_destroy(struct ps2_cdvd* cdvd) {
    ps2_cdvd_close(cdvd);

    free(cdvd->s_fifo);
    free(cdvd);
}

static const char* cdvd_extensions[] = {
    "iso",
    "cue",
    NULL
};

int cdvd_get_extension(const char* path) {
    if (!path)
        return CDVD_EXT_UNSUPPORTED;

    if (!(*path))
        return CDVD_EXT_UNSUPPORTED;

    const char* ptr = path + (strlen(path) - 1);

    while (*ptr != '.') {
        if (ptr == path)
            return CDVD_EXT_NONE;

        --ptr;
    }

    for (int i = 0; cdvd_extensions[i]; i++) {
        if (!strcmp(ptr + 1, cdvd_extensions[i])) {
            return i;
        }
    }

    return CDVD_EXT_UNSUPPORTED;
}

// To-do: Disc images
int ps2_cdvd_open(struct ps2_cdvd* cdvd, const char* path) {
    ps2_cdvd_close(cdvd);

    int ext = cdvd_get_extension(path);

    cdvd->file = fopen(path, "rb");

    if (!cdvd->file) {
        printf("cdvd: Cannot open disc image \"%s\"\n", path);

        exit(1);

        return 0;
    }

    // Read and verify PVD
    cdvd_read_sector(cdvd, 16, 0);

    if (strncmp(&cdvd->buf[1], "CD001", 5)) {
        printf("cdvd: File \'%s\' is not a valid ISO image\n", path);

        exit(1);

        return 0;
    }

    if ((*(uint16_t*)(cdvd->buf + 0x80)) != 0x800) {
        printf("cdvd: Unsupported image sector size %d\n", *((uint16_t*)(cdvd->buf + 0x80)));

        exit(1);

        return 0;
    }

    switch (ext) {
        case CDVD_EXT_ISO: cdvd->disc_type = 18; break;
        case CDVD_EXT_CUE: cdvd->disc_type = CDVD_DISC_PS2_CD; break;
    }

    cdvd->status &= ~CDVD_STATUS_TRAY_OPEN;

    return 1;
}

void ps2_cdvd_close(struct ps2_cdvd* cdvd) {
    if (cdvd->file) {
        fclose(cdvd->file);
    }

    cdvd->file = NULL;

    cdvd_set_status_bits(cdvd, CDVD_STATUS_TRAY_OPEN);
}

uint64_t ps2_cdvd_read8(struct ps2_cdvd* cdvd, uint32_t addr) {
    // printf("cdvd: read %08x\n", addr);

    switch (addr) {
        case 0x1F402004: /* printf("cdvd: read n_cmd %x\n", cdvd->n_cmd); */ return cdvd->n_cmd;
        case 0x1F402005: /* printf("cdvd: read n_stat %x\n", cdvd->n_stat); */ return cdvd->n_stat;
        // case 0x1F402005: (W)
        case 0x1F402006: /* printf("cdvd: read error %x\n", 0); */ return 0; //cdvd->error;
        // case 0x1F402007: (W)
        case 0x1F402008: /* printf("cdvd: read i_stat %x\n", cdvd->i_stat); */ return cdvd->i_stat;
        case 0x1F40200A: /* printf("cdvd: read status %x\n", cdvd->status); */ return cdvd->status;
        case 0x1F40200B: /* printf("cdvd: read sticky status %x\n", cdvd->sticky_status); */ return cdvd->sticky_status;
        case 0x1F40200F: /* printf("cdvd: read disc_type %x\n", cdvd->disc_type); */ return cdvd->disc_type;
        case 0x1F402016: /* printf("cdvd: read s_cmd %x\n", cdvd->s_cmd); */ return cdvd->s_cmd;
        case 0x1F402017: /* printf("cdvd: read s_stat %x\n", cdvd->s_stat); */ return cdvd->s_stat;
        // case 0x1F402017: (W);
        case 0x1F402018: uint8_t r = cdvd_read_s_response(cdvd); /* printf("cdvd: read s_response %x\n", r); */ return r;
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
        case 0x1F402007: printf("cdvd: break\n"); exit(1); /* To-do: BREAK */ return;
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