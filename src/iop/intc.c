#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "intc.h"

struct ps2_iop_intc* ps2_iop_intc_create(void) {
    return malloc(sizeof(struct ps2_iop_intc));
}

void ps2_iop_intc_init(struct ps2_iop_intc* intc, struct iop_state* iop) {
    memset(intc, 0, sizeof(struct ps2_iop_intc));

    intc->iop = iop;
    intc->ctrl = 1;
}

void ps2_iop_intc_irq(struct ps2_iop_intc* intc, int dev) {
    intc->stat |= dev;

    if (intc->ctrl && (intc->stat & intc->mask)) {
        iop_set_irq_pending(intc->iop);
    } else {
        intc->iop->cop0_r[COP0_CAUSE] &= ~SR_IM2;
    }
}

void ps2_iop_intc_destroy(struct ps2_iop_intc* intc) {
    free(intc);
}

uint64_t ps2_iop_intc_read8(struct ps2_iop_intc* intc, uint32_t addr) {
    printf("intc: IOP intc 8-bit read from address %08x\n", addr); exit(1);
}

uint64_t ps2_iop_intc_read16(struct ps2_iop_intc* intc, uint32_t addr) {
    printf("intc: IOP intc 16-bit read from address %08x\n", addr); exit(1);
}

uint64_t ps2_iop_intc_read32(struct ps2_iop_intc* intc, uint32_t addr) {
    uint32_t ctrl = intc->ctrl;

    switch (addr) {
        case 0x1f801070: return intc->stat;
        case 0x1f801074: return intc->mask;
        case 0x1f801078: intc->ctrl = 0; break;
    }

    int n = intc->ctrl && (intc->stat & intc->mask);

    if (!n) {
        intc->iop->cop0_r[COP0_CAUSE] &= ~SR_IM2;
    } else {
        iop_set_irq_pending(intc->iop);
    }

    return ctrl;
}

void ps2_iop_intc_write8(struct ps2_iop_intc* intc, uint32_t addr, uint64_t data) {
    printf("iop: IOP INTC 8-bit write to address %08x (%02lx)\n", addr, data); exit(1);
}

void ps2_iop_intc_write16(struct ps2_iop_intc* intc, uint32_t addr, uint64_t data) {
    printf("iop: IOP INTC 8-bit write to address %08x (%04lx)\n", addr, data); exit(1);
}

void ps2_iop_intc_write32(struct ps2_iop_intc* intc, uint32_t addr, uint64_t data) {
    switch (addr) {
        case 0x1f801070: intc->stat &= data; break;
        case 0x1f801074: intc->mask = data; break;
        case 0x1f801078: intc->ctrl = data; break;
    }

    int n = intc->ctrl && (intc->stat & intc->mask);

    if (!n) {
        intc->iop->cop0_r[COP0_CAUSE] &= ~SR_IM2;
    } else {
        iop_set_irq_pending(intc->iop);
    }
}