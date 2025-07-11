#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include "intc.h"

static inline void intc_check_irq(struct ps2_intc* intc) {
    ee_set_int0(intc->ee, intc->stat & intc->mask);
}

struct ps2_intc* ps2_intc_create(void) {
    return malloc(sizeof(struct ps2_intc));
}

void ps2_intc_init(struct ps2_intc* intc, struct ee_state* ee, struct sched_state* sched) {
    memset(intc, 0, sizeof(struct ps2_intc));

    intc->ee = ee;
    intc->sched = sched;
}

void ps2_intc_destroy(struct ps2_intc* intc) {
    free(intc);
}

uint64_t ps2_intc_read32(struct ps2_intc* intc, uint32_t addr) {
    switch (addr) {
        case 0x1000f000: return intc->stat;
        case 0x1000f010: return intc->mask;

        default: printf("intc: Unhandled INTC read %08x\n", addr); exit(1);
    }

    return 0;
}

void ps2_intc_write8(struct ps2_intc* intc, uint32_t addr, uint64_t data) {
    printf("intc: write8 %04lx\n", data); exit(1);

    switch (addr) {
        case 0x1000f000: intc->stat &= ~data; break;
        case 0x1000f010: intc->mask ^= data; break;

        default: printf("intc: Unhandled INTC write %08x %08lx\n", addr, data); exit(1);
    }

    intc_check_irq(intc);
}

void ps2_intc_write16(struct ps2_intc* intc, uint32_t addr, uint64_t data) {
    printf("intc: write16 %04lx\n", data); exit(1);

    switch (addr) {
        case 0x1000f000: intc->stat &= ~data; break;
        case 0x1000f010: intc->mask ^= data; break;

        default: printf("intc: Unhandled INTC write %08x %08lx\n", addr, data); exit(1);
    }

    intc_check_irq(intc);
}

void intc_check_irq_event(void* udata, int overshoot) {
    struct ps2_intc* intc = (struct ps2_intc*)udata;

    intc_check_irq(intc);
}

void ps2_intc_write32(struct ps2_intc* intc, uint32_t addr, uint64_t data) {
    switch (addr) {
        case 0x1000f000: intc->stat &= ~data; break;
        case 0x1000f010: intc->mask ^= data; break;

        default: printf("intc: Unhandled INTC write %08x %08lx\n", addr, data); exit(1);
    }

    struct sched_event event;

    event.callback = intc_check_irq_event;
    event.cycles = 16;
    event.name = "INTC IRQ check";
    event.udata = intc;

    sched_schedule(intc->sched, event);
}

void ps2_intc_write64(struct ps2_intc* intc, uint32_t addr, uint64_t data) {
    printf("intc: write64 %016lx\n", data); exit(1);

    switch (addr) {
        case 0x1000f000: intc->stat &= ~data; break;
        case 0x1000f010: intc->mask ^= data; break;

        default: printf("intc: Unhandled INTC write %08x %08lx\n", addr, data); exit(1);
    }

    intc_check_irq(intc);
}

void ps2_intc_irq(struct ps2_intc* intc, int dev) {
    intc->stat |= 1 << dev;

    struct sched_event event;

    event.callback = intc_check_irq_event;
    event.cycles = 64;
    event.name = "INTC IRQ check";
    event.udata = intc;

    sched_schedule(intc->sched, event);
}