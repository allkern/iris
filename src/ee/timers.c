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
        case 0x00: return timers->timer[t].counter & 0xffff;
        case 0x10: return timers->timer[t].mode;
        case 0x20: return timers->timer[t].compare;
        case 0x30: return timers->timer[t].hold;
    }

    return 0;
}

static inline void ee_timers_write_mode(struct ps2_ee_timers* timers, uint32_t data, int t) {
    struct ee_timer* timer = &timers->timer[t];

    timer->mode &= 0xc00;
    timer->mode |= data & (~0xc00);
    timer->mode &= ~(data & 0xc00);

    timer->clks = data & 3;
    timer->gate = (data >> 2) & 1;
    timer->gats = (data >> 3) & 1;
    timer->gatm = (data >> 4) & 3;
    timer->zret = (data >> 6) & 1;
    timer->cue = (data >> 7) & 1;
    timer->cmpe = (data >> 8) & 1;
    timer->ovfe = (data >> 9) & 1;

    if (timer->gate) {
        printf("timers: Timer %d gate write %08x\n", t, data);

        exit(1);
    }

    // printf("timers: Timer %d mode write %08x mode=%08x counter=%04x compare=%04x clks=%d gate=%d gats=%d gatm=%d zret=%d cue=%d cmpe=%d ovfe=%d\n",
    //     t, data,
    //     timer->mode,
    //     timer->counter,
    //     timer->compare,
    //     timer->clks, timer->gate, timer->gats, timer->gatm,
    //     timer->zret, timer->cue, timer->cmpe, timer->ovfe
    // );

    switch (timer->clks) {
        case 0: timer->delta = 1; break;
        case 1: timer->delta = 16; break;
        case 2: timer->delta = 256; break;
        case 3: timer->delta = 9370; break;
    }

    timer->delta_reload = timer->delta;

    if (timer->cmpe || timer->ovfe) {
        timer->check_enabled = 1;

        if (timer->counter < timer->compare) {
            timer->cycles_until_check = timer->compare - timer->counter;
        } else {
            timer->cycles_until_check = 0;
        }

        timer->check_reload = timer->cycles_until_check;

        printf("timer %d: compare/overflow IRQ enabled counter=%04x target=%04x\n", t, timer->counter, timer->compare);

        if (timer->ovfe)
            exit(1);
    }
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

int loop2 = 1;

void ps2_ee_timers_tick(struct ps2_ee_timers* timers) {
    for (int i = 0; i < 4; i++) {
        struct ee_timer* t = &timers->timer[i];

        if (!t->cue)
            continue;

        t->delta--;

        if (t->delta)
            continue;

        t->delta = t->delta_reload;
        t->counter++;
        t->counter &= 0xffff;

        if (t->check_enabled) {
            t->cycles_until_check--;

            if (t->cycles_until_check)
                continue;

            // printf("timer %d: counter=%04x target=%04x cycles_until_check=%04x\n", i, t->counter, t->compare, t->cycles_until_check);

            // if (!loop2--)
            //     exit(1);

            if (t->cmpe && t->counter == t->compare) {
                ps2_intc_irq(timers->intc, EE_INTC_TIMER0 + i);
            }

            if (t->zret) {
                t->counter = 0;
                t->cycles_until_check = t->compare;
            } else {
                t->cycles_until_check = 0x10000;
            }
        }
    }
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