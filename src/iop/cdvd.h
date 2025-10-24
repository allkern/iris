#ifndef CDVD_H
#define CDVD_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include "scheduler.h"
#include "dma.h"
#include "disc.h"

/*
    0     Tray status (1=open)
    1     Spindle spinning (1=spinning)
    2     Read status (1=reading data sectors)
    3     Paused
    4     Seek status (1=seeking)
    5     Error (1=error occurred)
    6-7   Unknown
*/
#define CDVD_STATUS_TRAY_OPEN_BIT 1
#define CDVD_STATUS_SPINNING_BIT  2
#define CDVD_STATUS_READING_BIT   4
#define CDVD_STATUS_PAUSED_BIT    8
#define CDVD_STATUS_SEEKING_BIT   16
#define CDVD_STATUS_ERROR_BIT     32
#define CDVD_STATUS_STOPPED       0x00
#define CDVD_STATUS_SPINNING      0x02
#define CDVD_STATUS_READING       0x06
#define CDVD_STATUS_PAUSED        0x0A
#define CDVD_STATUS_SEEKING       0x12

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
#define CDVD_IRQ_POWER_OFF    4
#define CDVD_IRQ_DISC_EJECTED 8
#define CDVD_IRQ_BS_POWER     16

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

struct nvram_layout {
	uint32_t bios_version;   // bios version that this eeprom layout is for
	int32_t config0_offset;   // offset of 1st config block
	int32_t config1_offset;   // offset of 2nd config block
	int32_t config2_offset;   // offset of 3rd config block
	int32_t console_id_offset; // offset of console id (?)
	int32_t ilink_id_offset;   // offset of ilink id (ilink mac address)
	int32_t modelnum_offset;  // offset of ps2 model number (eg "SCPH-70002")
	int32_t regparams_offset; // offset of RegionParams for PStwo
	int32_t mac_offset;       // offset of MAC address on PStwo
};

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

#ifdef _MSC_VER
    __declspec(align(4)) uint8_t n_params[16];
    __declspec(align(4)) uint8_t s_params[16];
#else
    uint8_t n_params[16] __attribute__((aligned(4)));
    uint8_t s_params[16] __attribute__((aligned(4)));
#endif

    uint8_t* s_fifo;
    int n_param_index;
    int s_param_index;
    int s_fifo_index;
    int s_fifo_size;

    uint8_t detected_disc_type;

    uint8_t mecha_decode;
    uint8_t cdkey[16];

    struct disc_state* disc;
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
    uint64_t layer2_lba;

    uint32_t config_rw;
    uint32_t config_offset;
    uint32_t config_numblocks;
    uint32_t config_block_index;

    // To-do:
    // void (*poweroff_handler)(void* udata)
    // void (*trayctrl_handler)(void* udata, uint8_t ctrl)
};

struct ps2_cdvd* ps2_cdvd_create(void);
void ps2_cdvd_init(struct ps2_cdvd* cdvd, struct ps2_iop_dma* dma, struct ps2_iop_intc* intc, struct sched_state* sched);
void ps2_cdvd_destroy(struct ps2_cdvd* cdvd);
int ps2_cdvd_open(struct ps2_cdvd* cdvd, const char* path, int delay);
void ps2_cdvd_close(struct ps2_cdvd* cdvd);
void ps2_cdvd_power_off(struct ps2_cdvd* cdvd);
uint64_t ps2_cdvd_read8(struct ps2_cdvd* cdvd, uint32_t addr);
void ps2_cdvd_write8(struct ps2_cdvd* cdvd, uint32_t addr, uint64_t data);
void ps2_cdvd_reset(struct ps2_cdvd* cdvd);

#undef ALIGNED_U32

#ifdef __cplusplus
}
#endif

#endif