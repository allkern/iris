#ifndef MCD_H
#define MCD_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdlib.h>

#include "iop/sio2.h"

#define MCD_SIZE_8MB 0x4000
#define MCD_SIZE_16MB 0x8000
#define MCD_SIZE_32MB 0x10000
#define MCD_SIZE_64MB 0x20000

// 512 bytes data + 16 bytes ECC
#define MCD_SECTOR_SIZE (512+16)

struct mcd_state {
    int port;
    uint8_t term;
    uint16_t buttons;
    uint8_t ax_right_y;
    uint8_t ax_right_x;
    uint8_t ax_left_y;
    uint8_t ax_left_x;
    int config_mode;
    int act_index;
    int mode_index;
    uint32_t size;
    uint8_t checksum;
    uint32_t addr;
    uint32_t buf_size;
    uint8_t* buf;

    FILE* file;
};

struct mcd_state* mcd_sio2_attach(struct ps2_sio2* sio2, int port, const char* path);
void mcd_sio2_detach(void* udata);

#ifdef __cplusplus
}
#endif

#endif