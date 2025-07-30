#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "timers.h"

struct ps2_ee_timers* ps2_ee_timers_create(void) {
    return malloc(sizeof(struct ps2_ee_timers));
}

void ps2_ee_timers_init(struct ps2_ee_timers* timers, struct ps2_intc* intc, struct sched_state* sched) {
    memset(timers, 0, sizeof(struct ps2_ee_timers));

    timers->intc = intc;
    timers->sched = sched;

    for (int i = 0; i < 4; i++) {
        timers->timer[i].compare = 0;
        timers->timer[i].mode = 0;
        timers->timer[i].counter = 0;
        timers->timer[i].hold = 0;
    }
}

void ps2_ee_timers_destroy(struct ps2_ee_timers* timers) {
    free(timers);
}

uint64_t ps2_ee_timers_read32(struct ps2_ee_timers* timers, uint32_t addr) {
    int t = (addr >> 11) & 3;

    // printf("ee: timer %d read %08x\n", t, addr & 0xff);

    switch (addr & 0xff) {
        case 0x00: return timers->timer[t].counter;
        case 0x10: return timers->timer[t].mode;
        case 0x20: return timers->timer[t].compare;
        case 0x30: return timers->timer[t].hold;
    }

    return 0;
}

static inline void ee_timers_write_mode(struct ps2_ee_timers* timers, uint32_t data, int t) {
    timers->timer[t].mode &= 0xc00;
    timers->timer[t].mode |= data & (~0xc00);
    timers->timer[t].mode &= ~(data & 0xc00);

    // printf("timers: timer %d write %08x -> %08x\n", t, data, timers->timer[t].mode);
}

void ps2_ee_timers_write32(struct ps2_ee_timers* timers, uint32_t addr, uint64_t data) {
    int t = (addr >> 11) & 3;

    // printf("ee: timer %d write %08x to %02x\n", t, data, addr & 0xff);

    switch (addr & 0xff) {
        case 0x00: timers->timer[t].counter = data & 0xffff; return;
        case 0x10: ee_timers_write_mode(timers, data & 0xffff, t); return;
        case 0x20: timers->timer[t].compare = data; return;
        case 0x30: timers->timer[t].hold = data & 0xffff; return;
    }
}

void ee_timer_tick(struct ps2_ee_timers* timers, int timer) {
    struct ee_timer* t = &timers->timer[timer];

    if (!(t->mode & 0x80))
        return;

    uint32_t prev = t->counter;

    switch (t->mode & 3) {
        case 0: {
            ++t->counter;
        } break;
        case 1: {
            if (t->internal >= 16) {
                ++t->counter;
                t->internal = 0;
            } else {
                ++t->internal;
            }
        } break;
        case 2: {
            if (t->internal >= 256) {
                ++t->counter;
                t->internal = 0;
            } else {
                ++t->internal;
            }
        } break;
        case 3: {
            if (t->internal >= 9370) {
                ++t->counter;
                t->internal = 0;
            } else {
                ++t->internal;
            }
        } break;
    }

    // printf("ee: timer %d counter=%08x\n", timer, t->counter);

    if ((t->counter >= t->compare) && (prev < t->compare)) {
        t->mode |= 0x400;

        if (t->mode & 0x100) {
            // printf("ee: timer %d compare IRQ\n", timer);

            ps2_intc_irq(timers->intc, EE_INTC_TIMER0 + timer);
        }

        if (t->mode & 0x40) {
            t->counter = 0;
        }
    }

    if (t->counter > 0xffff) {
        t->mode |= 0x800;
        t->counter -= 0xffff;

        if (t->mode & 0x200) {
            // printf("ee: timer %d overflow IRQ\n", timer);

            ps2_intc_irq(timers->intc, EE_INTC_TIMER0 + timer);
        }
    }

    t->counter &= 0xffff;
}

void ps2_ee_timers_tick(struct ps2_ee_timers* timers) {
    for (int i = 0; i < 4; i++)
        ee_timer_tick(timers, i);
}

void ps2_ee_timers_write16(struct ps2_ee_timers* timers, uint32_t addr, uint64_t data) {
    int t = (addr >> 11) & 3;

    switch (addr & 0xff) {
        case 0x00: timers->timer[t].counter = data & 0xffff; return;
        case 0x10: ee_timers_write_mode(timers, data & 0xffff, t); return;
        case 0x20: timers->timer[t].compare = data; return;
        case 0x30: timers->timer[t].hold = data & 0xffff; return;
    }

    printf("ee: timer %d write %08x to %02x\n", t, data, addr & 0xff);

    exit(1);
}

uint64_t ps2_ee_timers_read16(struct ps2_ee_timers* timers, uint32_t addr) {
    int t = (addr >> 11) & 3;

    // printf("ee: timer %d read %08x\n", t, addr & 0xff);

    switch (addr & 0xff) {
        case 0x00: return timers->timer[t].counter & 0xffff;
        case 0x10: return timers->timer[t].mode & 0xffff;
        case 0x20: return timers->timer[t].compare & 0xffff;
        case 0x30: return timers->timer[t].hold & 0xffff;
    }

    printf("ee: timers read16 %08x\n", addr);

    exit(1);

    return 0;
}