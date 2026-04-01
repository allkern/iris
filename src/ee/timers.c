#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "timers.h"

struct ps2_ee_timers* ps2_ee_timers_create(void) {
    return malloc(sizeof(struct ps2_ee_timers));
}

static inline void ee_timers_update_active_mask(struct ps2_ee_timers* timers, int t, int cue) {
    uint8_t bit = 1u << t;

    if (cue) {
        timers->active_mask |= bit;
    } else {
        timers->active_mask &= ~bit;
    }
}

void ps2_ee_timers_init(struct ps2_ee_timers* timers, struct ps2_intc* intc, struct sched_state* sched) {
    memset(timers, 0, sizeof(struct ps2_ee_timers));

    timers->intc = intc;
    timers->sched = sched;
    timers->current_cycle = 0;

    for (int i = 0; i < 4; i++) {
        timers->timer[i].id = i;
        timers->timer[i].last_sync_cycle = 0;
    }
}

static inline void ee_timers_update_event(struct ee_timer* t) {
    uint32_t counter = t->counter & 0xffff;
    uint32_t compare = t->compare & 0xffff;

    uint32_t cycles_until_compare;
    uint32_t cycles_until_overflow = 0x10000 - counter;

    if (counter < compare) {
        cycles_until_compare = compare - counter;
    } else {
        cycles_until_compare = 0x10000 - counter + compare;
    }

    // Compare events are needed for IRQ and for ZRET.
    if (!(t->cmpe || t->zret)) cycles_until_compare = 0x80000000;
    if (!t->ovfe) cycles_until_overflow = 0x80000000;

    t->cycles_until_check = cycles_until_compare < cycles_until_overflow ?
        cycles_until_compare : cycles_until_overflow;

    // printf("timer %d: cycles_until_check=%04x counter=%04x compare=%04x until_compare=%04x until_overflow=%04x\n",
    //     t, t->cycles_until_check, t->counter, t->compare, cycles_until_compare, cycles_until_overflow
    // );
}

static inline void ee_timers_sync_timer(struct ps2_ee_timers* timers, struct ee_timer* t, int i) {
    if (!t->cue) {
        t->last_sync_cycle = timers->current_cycle;
        return;
    }

    uint64_t cycles_since_sync = timers->current_cycle - t->last_sync_cycle;

    if (!cycles_since_sync)
        return;

    t->last_sync_cycle = timers->current_cycle;

    if (!t->check_enabled) {
        // Just advance counter without checking for events
        uint64_t remaining = cycles_since_sync;

        while (remaining > 0) {
            if (remaining >= t->delta) {
                remaining -= t->delta;
                t->counter = (t->counter + 1) & 0xffff;
                t->delta = t->delta_reload;
            } else {
                t->delta -= remaining;
                remaining = 0;
            }
        }
        return;
    }

    // Need to check for compare and overflow events
    uint64_t remaining_cycles = cycles_since_sync;

    while (remaining_cycles > 0) {
        if (remaining_cycles >= t->delta) {
            remaining_cycles -= t->delta;
            t->delta = t->delta_reload;

            uint32_t counter = (t->counter + 1) & 0xffff;
            uint32_t low_counter = counter & 0xffff;
            int cmp = low_counter == (t->compare & 0xffff);
            int ovf = counter == 0x10000;

            if (cmp) {
                if (t->cmpe)
                    t->mode |= 0x400;

                if (t->zret) {
                    counter = 0;
                }

                if (t->cmpe) {
                    ps2_intc_irq(timers->intc, EE_INTC_TIMER0 + i);
                }
            }

            if (counter == 0x10000) {
                if (t->ovfe)
                    t->mode |= 0x800;

                if (t->ovfe) {
                    ps2_intc_irq(timers->intc, EE_INTC_TIMER0 + i);
                }
                counter = 0;
            }

            t->counter = counter & 0xffff;
        } else {
            t->delta -= remaining_cycles;
            remaining_cycles = 0;
        }
    }

    ee_timers_update_event(t);
}

void ee_timers_write_counter(struct ps2_ee_timers* timers, int t, uint32_t data) {
    struct ee_timer* timer = &timers->timer[t];

    // Sync to current cycle first
    ee_timers_sync_timer(timers, timer, t);

    // printf("timer %d: write counter=%04x data=%04x\n", t, timer->counter, data);

    timer->counter = data;
    timer->delta = timer->delta_reload;

    if (timer->check_enabled) {
        ee_timers_update_event(timer);
    }
}

void ee_timers_write_compare(struct ps2_ee_timers* timers, int t, uint32_t data) {
    struct ee_timer* timer = &timers->timer[t];

    // Sync to current cycle first
    ee_timers_sync_timer(timers, timer, t);

    // printf("timer %d: write counter=%04x data=%04x\n", t, timer->counter, data);

    if (data < timer->counter) {
        // printf("timer %d: compare %04x >= counter %04x\n", t, data, timer->counter);

        // exit(1);
    } else if (data == timer->counter) {
        // printf("timer %d: compare %04x == counter %04x\n", t, data, timer->counter);

        // exit(1);
    }

    // timer->cycles_until_check = data - timer->counter;
    timer->compare = data;

    if (timer->check_enabled) {
        ee_timers_update_event(timer);
    }
}

void ps2_ee_timers_destroy(struct ps2_ee_timers* timers) {
    free(timers);
}

static inline void ee_timers_write_mode(struct ps2_ee_timers* timers, int t, uint32_t data) {
    struct ee_timer* timer = &timers->timer[t];

    // Sync to current cycle first before changing mode
    ee_timers_sync_timer(timers, timer, t);

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

    ee_timers_update_active_mask(timers, t, timer->cue);

    if (!timer->cue) {
        timer->check_enabled = 0;
        return;
    }

    // Reset sync point when timer is enabled
    timer->last_sync_cycle = timers->current_cycle;

    if (timer->gate) {
        printf("timers: Timer %d gate write %08x\n", t, data);

        // exit(1);
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

    if (timer->cmpe || timer->ovfe || timer->zret) {
        timer->check_enabled = 1;

        ee_timers_update_event(timer);
    } else {
        timer->check_enabled = 0;
    }
}

void ps2_ee_timers_tick(struct ps2_ee_timers* timers) {
    ps2_ee_timers_tick_cycles(timers, 1);
}

void ps2_ee_timers_tick_cycles(struct ps2_ee_timers* timers, uint32_t cycles) {
    // Lazy evaluation: just advance the global cycle counter
    // No timer updates happen until reads/writes or event checks
    if (timers->active_mask && cycles) {
        timers->current_cycle += cycles;
    }
}

void ps2_ee_timers_write16(struct ps2_ee_timers* timers, uint32_t addr, uint64_t data) {
    int t = (addr >> 11) & 3;

    switch (addr & 0xff) {
        case 0x00: ee_timers_write_counter(timers, t, data & 0xffff); return;
        case 0x10: ee_timers_write_mode(timers, t, data & 0xffff); return;
        case 0x20: ee_timers_write_compare(timers, t, data & 0xffff); return;
        case 0x30: timers->timer[t].hold = data & 0xffff; return;
    }

    fprintf(stderr, "ee: timer %d write %08x to %02x\n", t, data, addr & 0xff);

    exit(1);
}

void ps2_ee_timers_write32(struct ps2_ee_timers* timers, uint32_t addr, uint64_t data) {
    int t = (addr >> 11) & 3;

    // printf("ee: timer %d write %08x to %02x\n", t, data, addr & 0xff);

    switch (addr & 0xff) {
        case 0x00: ee_timers_write_counter(timers, t, data & 0xffff); return;
        case 0x10: ee_timers_write_mode(timers, t, data & 0xffff); return;
        case 0x20: ee_timers_write_compare(timers, t, data & 0xffff); return;
        case 0x30: timers->timer[t].hold = data & 0xffff; return;
    }
}

uint64_t ps2_ee_timers_read16(struct ps2_ee_timers* timers, uint32_t addr) {
    int t = (addr >> 11) & 3;

    // printf("ee: timer %d read %08x\n", t, addr & 0xff);

    // Sync timer to current cycle before reading
    ee_timers_sync_timer(timers, &timers->timer[t], t);

    switch (addr & 0xff) {
        case 0x00: return timers->timer[t].counter & 0xffff;
        case 0x10: return timers->timer[t].mode & 0xffff;
        case 0x20: return timers->timer[t].compare & 0xffff;
        case 0x30: return timers->timer[t].hold & 0xffff;
    }

    fprintf(stderr, "ee: timers read16 %08x\n", addr);

    exit(1);

    return 0;
}

uint64_t ps2_ee_timers_read32(struct ps2_ee_timers* timers, uint32_t addr) {
    int t = (addr >> 11) & 3;

    // printf("ee: timer %d read %08x\n", t, addr & 0xff);

    // Sync timer to current cycle before reading
    ee_timers_sync_timer(timers, &timers->timer[t], t);

    switch (addr & 0xff) {
        case 0x00: return timers->timer[t].counter & 0xffff;
        case 0x10: return timers->timer[t].mode;
        case 0x20: return timers->timer[t].compare;
        case 0x30: return timers->timer[t].hold;
    }

    return 0;
}
