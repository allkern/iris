#ifndef SBUS_H
#define SBUS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "ee/intc.h"
#include "iop/intc.h"
#include "sched.h"

struct ps2_sbus {
    struct ps2_intc* ee_intc;
    struct ps2_iop_intc* iop_intc;
    struct sched_state* sched;
};

struct ps2_sbus* ps2_sbus_create(void);
void ps2_sbus_init(struct ps2_sbus* sbus, struct ps2_intc* ee_intc, struct ps2_iop_intc* iop_intc, struct sched_state* sched);
void ps2_sbus_destroy(struct ps2_sbus* sbus);
uint64_t ps2_sbus_read8(struct ps2_sbus* sbus, uint32_t addr);
uint64_t ps2_sbus_read16(struct ps2_sbus* sbus, uint32_t addr);
uint64_t ps2_sbus_read32(struct ps2_sbus* sbus, uint32_t addr);
void ps2_sbus_write8(struct ps2_sbus* sbus, uint32_t addr, uint64_t data);
void ps2_sbus_write16(struct ps2_sbus* sbus, uint32_t addr, uint64_t data);
void ps2_sbus_write32(struct ps2_sbus* sbus, uint32_t addr, uint64_t data);

#ifdef __cplusplus
}
#endif

#endif