#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "timers.h"

#define EE_TIMER_SCHED_QUANTUM 64
#define EE_TO_BUSCLK_SHIFT 1

static void ee_timers_schedule_next_irq_event(struct ps2_ee_timers* timers);

static inline uint64_t ee_timers_ee_cycles_to_busclk(struct ps2_ee_timers* timers, uint64_t ee_cycles) {
    uint64_t total = ee_cycles + timers->ee_cycle_phase;

    timers->ee_cycle_phase = (uint8_t)(total & ((1u << EE_TO_BUSCLK_SHIFT) - 1));

    return total >> EE_TO_BUSCLK_SHIFT;
}

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
    timers->scheduler_advanced_cycles = 0;
    timers->irq_event_pending = 0;
    timers->ee_cycle_phase = 0;

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

static inline uint32_t ee_timers_cycles_until_check(struct ee_timer* t) {
    if (!t->cue || !t->check_enabled)
        return 0xffffffffu;

    uint32_t period = t->delta_reload;

    if (!period)
        return 0xffffffffu;

    uint64_t cycles = t->delta;

    if (t->cycles_until_check > 1) {
        cycles += (uint64_t)(t->cycles_until_check - 1) * period;
    }

    if (!cycles)
        cycles = 1;

    if (cycles > 0x7fffffffu)
        cycles = 0x7fffffffu;

    return (uint32_t)cycles;
}

static inline void ee_timers_advance_counter(struct ps2_ee_timers* timers, struct ee_timer* t, int i, uint32_t increments) {
    if (!increments)
        return;

    if (!t->check_enabled) {
        t->counter = (t->counter + increments) & 0xffff;
        return;
    }

    while (increments) {
        if (t->cycles_until_check > 1) {
            uint32_t skip = t->cycles_until_check - 1;

            if (skip > increments)
                skip = increments;

            t->counter = (t->counter + skip) & 0xffff;
            t->cycles_until_check -= skip;
            increments -= skip;

            if (!increments)
                break;
        }

        uint32_t counter = t->counter + 1;
        uint32_t low_counter = counter & 0xffff;
        int cmp = low_counter == (t->compare & 0xffff);
        int ovf = counter == 0x10000;

        if (!(cmp || ovf)) {
            fprintf(stderr, "timer %d: error counter=%04x compare=%04x cycles_until-check=%d\n", i, counter, t->compare, t->cycles_until_check);

            exit(1);
        }

        if (cmp) {
            if (t->cmpe)
                t->mode |= 0x400;

            if (t->zret) {
                counter = 0;
            }

            if (t->cmpe) {
                // printf("timer %d: compare match counter=%04x compare=%04x\n", i, counter, t->compare);

                ps2_intc_irq(timers->intc, EE_INTC_TIMER0 + i);
            }

            t->counter = counter;
            ee_timers_update_event(t);
            counter = t->counter;
        }

        if (counter == 0x10000) {
            if (t->ovfe)
                t->mode |= 0x800;

            if (t->ovfe) {
                // printf("timer %d: overflow match counter=%04x compare=%04x\n", i, counter, t->compare);

                ps2_intc_irq(timers->intc, EE_INTC_TIMER0 + i);
            }

            t->counter = counter;
            ee_timers_update_event(t);
            counter = t->counter;
        }

        t->counter = counter & 0xffff;
        increments--;
    }
}

static inline void ee_timers_sync_timer(struct ps2_ee_timers* timers, struct ee_timer* t, int i) {
    if (!t->cue) {
        t->last_sync_cycle = timers->current_cycle;
        return;
    }

    // CLKS=3 uses HBLANK as the clock source, so it is stepped by
    // ps2_ee_timers_handle_hblank rather than EE/BUSCLK cycle deltas.
    if (t->clks == 3) {
        t->last_sync_cycle = timers->current_cycle;
        return;
    }

    uint64_t cycles_since_sync = timers->current_cycle - t->last_sync_cycle;

    if (!cycles_since_sync)
        return;

    t->last_sync_cycle = timers->current_cycle;

    if (cycles_since_sync < t->delta) {
        t->delta -= (uint32_t)cycles_since_sync;
        return;
    }

    cycles_since_sync -= t->delta;

    uint32_t increments = 1;
    uint32_t period = t->delta_reload;

    if (period) {
        increments += (uint32_t)(cycles_since_sync / period);

        uint32_t rem = (uint32_t)(cycles_since_sync % period);
        t->delta = period - rem;

        if (t->delta == 0)
            t->delta = period;
    } else {
        t->delta = 0;
    }

    ee_timers_advance_counter(timers, t, i, increments);
}

static void ee_timers_irq_event_cb(void* udata, int overshoot) {
    struct ps2_ee_timers* timers = (struct ps2_ee_timers*)udata;
    uint64_t elapsed_ee_cycles = EE_TIMER_SCHED_QUANTUM;

    if (timers->sched && timers->sched->offset)
        elapsed_ee_cycles = timers->sched->offset;

    if (overshoot < 0)
        elapsed_ee_cycles += (uint64_t)(-overshoot);

    uint64_t elapsed_busclk_cycles = ee_timers_ee_cycles_to_busclk(timers, elapsed_ee_cycles);

    timers->irq_event_pending = 0;

    timers->current_cycle += elapsed_busclk_cycles;
    timers->scheduler_advanced_cycles += elapsed_busclk_cycles;

    for (int i = 0; i < 4; i++) {
        if (timers->timer[i].cue) {
            ee_timers_sync_timer(timers, &timers->timer[i], i);
        }
    }

    ee_timers_schedule_next_irq_event(timers);
}

static void ee_timers_schedule_next_irq_event(struct ps2_ee_timers* timers) {
    if (!timers->sched)
        return;

    if (timers->irq_event_pending)
        return;

    uint32_t min_cycles = 0xffffffffu;

    for (int i = 0; i < 4; i++) {
        if (timers->timer[i].cue) {
            ee_timers_sync_timer(timers, &timers->timer[i], i);

            uint32_t wait = ee_timers_cycles_until_check(&timers->timer[i]);

            if (wait < min_cycles)
                min_cycles = wait;
        }
    }

    if (min_cycles == 0xffffffffu)
        return;

    if (min_cycles > EE_TIMER_SCHED_QUANTUM)
        min_cycles = EE_TIMER_SCHED_QUANTUM;

    uint64_t sched_cycles = (uint64_t)min_cycles << EE_TO_BUSCLK_SHIFT;

    struct sched_event event;
    event.name = "EE Timer IRQ";
    event.udata = timers;
    event.callback = ee_timers_irq_event_cb;
    event.cycles = (long)sched_cycles;

    sched_schedule(timers->sched, event);
    timers->irq_event_pending = 1;
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

    ee_timers_schedule_next_irq_event(timers);
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

    ee_timers_schedule_next_irq_event(timers);
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

    // if (t == 0)
    // printf("timers: Timer %d mode write %08x mode=%08x counter=%04x compare=%04x clks=%d gate=%d gats=%d gatm=%d zret=%d cue=%d cmpe=%d ovfe=%d\n",
    //     t, data,
    //     timer->mode,
    //     timer->counter,
    //     timer->compare,
    //     timer->clks, timer->gate, timer->gats, timer->gatm,
    //     timer->zret, timer->cue, timer->cmpe, timer->ovfe
    // );

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

    // if (t == 0)
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

    ee_timers_schedule_next_irq_event(timers);
}

void ps2_ee_timers_tick(struct ps2_ee_timers* timers) {
    ps2_ee_timers_tick_cycles(timers, 1);
}

void ps2_ee_timers_tick_cycles(struct ps2_ee_timers* timers, uint32_t cycles) {
    // Lazy evaluation: just advance the global cycle counter
    // No timer updates happen until reads/writes or event checks
    if (timers->active_mask && cycles) {
        uint64_t step = ee_timers_ee_cycles_to_busclk(timers, cycles);

        if (timers->scheduler_advanced_cycles) {
            if (timers->scheduler_advanced_cycles >= step) {
                timers->scheduler_advanced_cycles -= step;
                step = 0;
            } else {
                step -= timers->scheduler_advanced_cycles;
                timers->scheduler_advanced_cycles = 0;
            }
        }

        timers->current_cycle += step;
    }
}

void ps2_ee_timers_handle_hblank(struct ps2_ee_timers* timers) {
    if (!timers)
        return;

    for (int i = 0; i < 4; i++) {
        struct ee_timer* t = &timers->timer[i];

        if (!t->cue)
            continue;

        if (t->clks != 3)
            continue;

        ee_timers_advance_counter(timers, t, i, 1);
    }
}

void ps2_ee_timers_handle_vblank_in(struct ps2_ee_timers* timers) {
    (void)timers;
}

void ps2_ee_timers_handle_vblank_out(struct ps2_ee_timers* timers) {
    (void)timers;
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
