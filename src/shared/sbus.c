#include <stdlib.h>
#include <string.h>

#include "sbus.h"

struct ps2_sbus* ps2_sbus_create(void) {
    return malloc(sizeof(struct ps2_sbus));
}

void ps2_sbus_init(struct ps2_sbus* sbus, struct ps2_intc* ee_intc, struct ps2_iop_intc* iop_intc, struct sched_state* sched) {
    memset(sbus, 0, sizeof(struct ps2_sbus));

    sbus->ee_intc = ee_intc;
    sbus->iop_intc = iop_intc;
    sbus->sched = sched;
}

void ps2_sbus_destroy(struct ps2_sbus* sbus) {
    free(sbus);
}

uint64_t ps2_sbus_read8(struct ps2_sbus* sbus, uint32_t addr) {
    printf("sbus: 8-bit read %08x\n", addr); exit(1);
}

uint64_t ps2_sbus_read16(struct ps2_sbus* sbus, uint32_t addr) {
    printf("sbus: 16-bit read %08x\n", addr); exit(1);
}

void sbus_trigger_iop_irq(void* udata, int overshoot) {
    struct ps2_sbus* sbus = (struct ps2_sbus*)udata;

    ps2_iop_intc_irq(sbus->iop_intc, IOP_INTC_SBUS);
}

void ps2_sbus_write8(struct ps2_sbus* sbus, uint32_t addr, uint64_t data) {
    printf("sbus: 8-bit write %08x <- %02lx\n", addr, data); exit(1);
}

void ps2_sbus_write16(struct ps2_sbus* sbus, uint32_t addr, uint64_t data) {
    printf("sbus: 16-bit write %08x <- %04lx\n", addr, data); exit(1);
}

void ps2_sbus_write32(struct ps2_sbus* sbus, uint32_t addr, uint64_t data) {
    switch (addr) {
        case 0x1f801450: {
            if (data & 2) {
                ps2_intc_irq(sbus->ee_intc, EE_INTC_SBUS);
            }
        } return;
    }
}