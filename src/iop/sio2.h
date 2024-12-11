#ifndef SIO2_H
#define SIO2_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include "queue.h"
#include "intc.h"

struct ps2_sio2 {
    uint32_t ctrl;
    uint32_t send3[16];
    uint32_t send1[8];
    uint32_t send2[8];
    struct queue_state* in;
    struct queue_state* out;
    uint32_t recv1;
    uint32_t recv2;
    uint32_t recv3;
    uint32_t istat;

    struct ps2_iop_intc* intc;
};

struct ps2_sio2* ps2_sio2_create(void);
void ps2_sio2_init(struct ps2_sio2* sio2, struct ps2_iop_intc* intc);
void ps2_sio2_destroy(struct ps2_sio2* sio2);
uint64_t ps2_sio2_read8(struct ps2_sio2* sio2, uint32_t addr);
uint64_t ps2_sio2_read32(struct ps2_sio2* sio2, uint32_t addr);
void ps2_sio2_write8(struct ps2_sio2* sio2, uint32_t addr, uint64_t data);
void ps2_sio2_write32(struct ps2_sio2* sio2, uint32_t addr, uint64_t data);

#ifdef __cplusplus
}
#endif

#endif