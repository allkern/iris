#ifndef PS1_MCD_H
#define PS1_MCD_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdlib.h>

#include "iop/sio2.h"

// 128 bytes data
#define PS1_MCD_SECTOR_SIZE (128)
#define PS1_MCD_SIZE 0x20000

struct ps1_mcd_state {
    // 128 KiB
    uint8_t buf[PS1_MCD_SIZE];
    uint8_t flag;

    FILE* file;
};

struct ps1_mcd_state* ps1_mcd_attach(struct ps2_sio2* sio2, int port, const char* path);
void ps1_mcd_detach(void* udata);

#ifdef __cplusplus
}
#endif

#endif