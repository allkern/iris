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

    printf("ee: timer %d read %08x\n", t, addr & 0xff);
    switch (addr & 0xff) {
        case 0x00: return timers->timer[t].counter;
        case 0x10: return timers->timer[t].mode;
        case 0x20: return timers->timer[t].compare;
        case 0x30: return timers->timer[t].hold;
    }

    return 0;
}

void ps2_ee_timers_write32(struct ps2_ee_timers* timers, uint32_t addr, uint64_t data) {
    int t = (addr >> 11) & 3;

    printf("ee: timer %d write %08x to %02x\n", t, data, addr & 0xff);

    switch (addr & 0xff) {
        case 0x00: timers->timer[t].counter = data; return;
        case 0x10: timers->timer[t].mode = data; return;
        case 0x20: timers->timer[t].compare = data; return;
        case 0x30: timers->timer[t].hold = data; return;
    }
}

void ee_timer_tick(struct ps2_ee_timers* timers, int timer) {
    struct ee_timer* t = &timers->timer[timer];

    if (!(t->mode & 0x80))
        return;

    uint32_t prev = t->counter;

    t->counter += 1;

    // printf("ee: timer %d counter=%08x\n", timer, t->counter);

    if ((t->counter >= t->compare) && (prev < t->compare)) {
        t->mode |= 0x400;

        if (t->mode & 0x100) {
            // printf("ee: Sending compare IRQ\n");
            ps2_intc_irq(timers->intc, EE_INTC_TIMER0 << timer);
        }

        if (t->mode & 0x40) {
            t->counter = 0;
        }
    }

    if (t->counter > 0xffff) {
        t->mode |= 0x800;
        t->counter -= 0xffff;

        if (t->mode & 0x200) {
            ps2_intc_irq(timers->intc, EE_INTC_TIMER0 << timer);
        }
    }

    t->counter &= 0xffff;
}

void ps2_ee_timers_tick(struct ps2_ee_timers* timers) {
    for (int i = 0; i < 4; i++)
        ee_timer_tick(timers, i);
}