struct ps2_intc;

#ifndef INTC_H
#define INTC_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include "u128.h"
#include "ee.h"
#include "scheduler.h"

#define EE_INTC_GS         0
#define EE_INTC_SBUS       1
#define EE_INTC_VBLANK_IN  2
#define EE_INTC_VBLANK_OUT 3
#define EE_INTC_VIF0       4
#define EE_INTC_VIF1       5
#define EE_INTC_VU0        6
#define EE_INTC_VU1        7
#define EE_INTC_IPU        8
#define EE_INTC_TIMER0     9
#define EE_INTC_TIMER1     10
#define EE_INTC_TIMER2     11
#define EE_INTC_TIMER3     12
#define EE_INTC_SFIFO      13
#define EE_INTC_VU0_WD     14

struct ps2_intc {
    uint32_t stat;
    uint32_t mask;

    struct ee_state* ee;
    struct sched_state* sched;
};

struct ps2_intc* ps2_intc_create(void);
void ps2_intc_init(struct ps2_intc* intc, struct ee_state* ee, struct sched_state* sched);
void ps2_intc_destroy(struct ps2_intc* intc);
uint64_t ps2_intc_read32(struct ps2_intc* intc, uint32_t addr);
void ps2_intc_write8(struct ps2_intc* intc, uint32_t addr, uint64_t data);
void ps2_intc_write16(struct ps2_intc* intc, uint32_t addr, uint64_t data);
void ps2_intc_write32(struct ps2_intc* intc, uint32_t addr, uint64_t data);
void ps2_intc_write64(struct ps2_intc* intc, uint32_t addr, uint64_t data);
void ps2_intc_irq(struct ps2_intc* intc, int dev);

#ifdef __cplusplus
}
#endif

#endif