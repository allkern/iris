#ifndef CDVD_H
#define CDVD_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include "sched.h"
#include "dma.h"

#define CDVD_ERR_CANT_OPEN 0
#define CDVD_ERR_ISO_INVALID 1
#define CDVD_ERR_UNSUPPORTED_SS 2

#define CDVD_EXT_ISO 0
#define CDVD_EXT_BIN 1
#define CDVD_EXT_CUE 2
#define CDVD_EXT_NONE 3
#define CDVD_EXT_UNSUPPORTED 4

/*
    0     Tray status (1=open)
    1     Spindle spinning (1=spinning)
    2     Read status (1=reading data sectors)
    3     Paused
    4     Seek status (1=seeking)
    5     Error (1=error occurred)
    6-7   Unknown
*/
#define CDVD_STATUS_TRAY_OPEN 1
#define CDVD_STATUS_SPINNING  2
#define CDVD_STATUS_READING   4
#define CDVD_STATUS_PAUSED    8
#define CDVD_STATUS_SEEKING   16
#define CDVD_STATUS_ERROR     32

/*
    0     Error (1=error occurred)
    1     Unknown/unused
    2     DEV9 device connected (1=HDD/network adapter connected)
    3     Unknown/unused
    4     Test mode
    5     Power off ready
    6     Drive status (1=ready)
    7     Busy executing NCMD
*/
#define CDVD_N_STATUS_ERROR          1
#define CDVD_N_STATUS_DEV9_CONNECTED 4
#define CDVD_N_STATUS_TEST_MODE      16
#define CDVD_N_STATUS_POWER_OFF      32
#define CDVD_N_STATUS_READY          64
#define CDVD_N_STATUS_BUSY           128

/*
    0     Data ready?
    1     (N?) Command complete
    2     Power off pressed
    3     Disk ejected
    4     BS_Power DET?
    5-7   Unused
*/
#define CDVD_IRQ_DATA_READY   1
#define CDVD_IRQ_NCMD_DONE    2
#define CDVD_IRQ_DISC_EJECTED 4
#define CDVD_IRQ_POWER_OFF    8
#define CDVD_IRQ_BS_POWER     16

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

/*
    0-5   Unknown
    6     Result data available (0=available, 1=no data)
    7     Busy
*/
#define CDVD_S_STATUS_NO_DATA 64
#define CDVD_S_STATUS_BUSY    128

#define CDVD_CD_SS_2328 2328
#define CDVD_CD_SS_2340 2340
#define CDVD_CD_SS_2048 2048
#define CDVD_DVD_SS 2064

struct ps2_cdvd {
    uint8_t n_cmd;
    uint8_t n_stat;
    uint8_t error;
    uint8_t i_stat;
    uint8_t status;
    uint8_t sticky_status;
    uint8_t disc_type;
    uint8_t s_cmd;
    uint8_t s_stat;
    uint8_t n_params[16];
    uint8_t s_params[16];
    uint8_t* s_fifo;
    int n_param_index;
    int s_param_index;
    int s_fifo_index;
    int s_fifo_size;

    FILE* file;
    uint8_t buf[2340];
    int buf_size;

    // Pending read
    uint32_t read_lba;
    uint32_t read_count;
    uint32_t read_size;
    uint8_t read_speed;

    uint8_t nvram[1024];

    struct ps2_iop_dma* dma;
    struct ps2_iop_intc* intc;
    struct sched_state* sched;

    // Dual-layer DVDs
    uint32_t layer2_lba;
};

struct ps2_cdvd* ps2_cdvd_create(void);
void ps2_cdvd_init(struct ps2_cdvd* cdvd, struct ps2_iop_dma* dma, struct ps2_iop_intc* intc, struct sched_state* sched);
void ps2_cdvd_destroy(struct ps2_cdvd* cdvd);
int ps2_cdvd_open(struct ps2_cdvd* cdvd, const char* path);
void ps2_cdvd_close(struct ps2_cdvd* cdvd);
uint64_t ps2_cdvd_read8(struct ps2_cdvd* cdvd, uint32_t addr);
void ps2_cdvd_write8(struct ps2_cdvd* cdvd, uint32_t addr, uint64_t data);

#ifdef __cplusplus
}
#endif

#endif