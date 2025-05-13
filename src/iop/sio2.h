#ifndef SIO2_H
#define SIO2_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include "queue.h"
#include "intc.h"
#include "sched.h"
#include "dma.h"

#define SIO2_DEV_PAD  0x01
#define SIO2_DEV_PS1PAD 0x42 // ?
#define SIO2_DEV_MTAP 0x21
#define SIO2_DEV_IR   0x61
#define SIO2_DEV_MCD  0x81

struct ps2_sio2;

struct sio2_device {
    void (*handle_command)(struct ps2_sio2*, void*);
    void* udata;
};

struct ps2_sio2 {
    struct sio2_device port[4];

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

    int send3_index;

    struct ps2_iop_dma* dma;
    struct ps2_iop_intc* intc;
    struct sched_state* sched;
};

struct ps2_sio2* ps2_sio2_create(void);
void ps2_sio2_init(struct ps2_sio2* sio2, struct ps2_iop_dma* dma, struct ps2_iop_intc* intc, struct sched_state* sched);
void ps2_sio2_destroy(struct ps2_sio2* sio2);
uint64_t ps2_sio2_read8(struct ps2_sio2* sio2, uint32_t addr);
uint64_t ps2_sio2_read32(struct ps2_sio2* sio2, uint32_t addr);
void ps2_sio2_write8(struct ps2_sio2* sio2, uint32_t addr, uint64_t data);
void ps2_sio2_write32(struct ps2_sio2* sio2, uint32_t addr, uint64_t data);
void ps2_sio2_attach_device(struct ps2_sio2* sio2, struct sio2_device dev, int port);
void ps2_sio2_detach_device(struct ps2_sio2* sio2, int port);
void sio2_dma_reset(struct ps2_sio2* sio2);

#ifdef __cplusplus
}
#endif

#endif