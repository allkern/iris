#ifndef IPU_H
#define IPU_H

#ifdef __cplusplus
extern "C" {
#endif

#include "ee/dmac.h"
#include "ee/intc.h"

struct ps2_ipu;

struct ps2_ipu* ps2_ipu_create(void);
void ps2_ipu_init(struct ps2_ipu* ipu, struct ps2_dmac* dmac, struct ps2_intc* intc);
void ps2_ipu_reset(struct ps2_ipu* ipu);
uint64_t ps2_ipu_read64(struct ps2_ipu* ipu, uint32_t addr);
uint128_t ps2_ipu_read128(struct ps2_ipu* ipu, uint32_t addr);
void ps2_ipu_write64(struct ps2_ipu* ipu, uint32_t addr, uint64_t data);
void ps2_ipu_write128(struct ps2_ipu* ipu, uint32_t addr, uint128_t data);
void ps2_ipu_destroy(struct ps2_ipu* ipu);

#ifdef __cplusplus
}
#endif

#endif