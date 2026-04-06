#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>

#include "timers.h"
#include "intc.h"
#include "scheduler.h"

static void iop_timers_recompute_next_check(struct ps2_iop_timers* timers);

struct ps2_iop_timers* ps2_iop_timers_create(void) {
    return malloc(sizeof(struct ps2_iop_timers));
}

void ps2_iop_timers_init(struct ps2_iop_timers* timers, struct ps2_iop_intc* intc, struct sched_state* sched) {
    memset(timers, 0, sizeof(struct ps2_iop_timers));

    timers->intc = intc;
    timers->sched = sched;
    timers->active_mask = 0x3f;
    timers->next_check_cycle = 1;

    for (int i = 0; i < 6; i++) {
        timers->timer[i].step = 2;
        timers->timer[i].delta = 1;
        timers->timer[i].delta_reload = 1;
        timers->timer[i].cycles_until_check = 1;
        timers->timer[i].check_enabled = 1;
    }
}

void ps2_iop_timers_destroy(struct ps2_iop_timers* timers) {
    free(timers);
}

static inline uint32_t timer_get_irq_mask(int t) {
    switch (t) {
        case 0: return IOP_INTC_TIMER0;
        case 1: return IOP_INTC_TIMER1;
        case 2: return IOP_INTC_TIMER2;
        case 3: return IOP_INTC_TIMER3;
        case 4: return IOP_INTC_TIMER4;
        case 5: return IOP_INTC_TIMER5;
    }

    return 0;
}

static inline uint64_t iop_timer_ovf_limit(int i) {
    return (i < 3) ? 0xffffu : 0xffffffffu;
}

static inline void iop_timer_refresh_clock(struct iop_timer* t, int i) {
    uint32_t period = 1;
    uint8_t step = 2;

    if ((i == 1) && t->use_ext) {
        period = 91;
        step = 1;
    } else if ((i == 4) && t->t4_prescaler) {
        period = 129;
        step = 1;
    }

    t->step = step;
    t->delta_reload = period;

    if (!t->delta || (t->delta > period)) {
        uint32_t phase = (uint32_t)t->internal;

        if (phase >= period)
            phase %= period;

        t->delta = period - phase;

        if (!t->delta)
            t->delta = period;
    }
}

static inline void iop_timer_update_internal_from_delta(struct iop_timer* t) {
    if (t->delta_reload <= 1) {
        t->internal = 0;
        return;
    }

    t->internal = (int64_t)(t->delta_reload - t->delta);
}

static inline void iop_timer_update_event(struct iop_timer* t, int i) {
    if (!t->check_enabled) {
        t->cycles_until_check = UINT64_MAX;
        return;
    }

    uint64_t cmp_increments = UINT64_MAX;
    uint64_t ovf_increments = 1;
    uint64_t step = t->step ? t->step : 1;
    uint64_t ovf = iop_timer_ovf_limit(i);

    if (t->counter < (int64_t)t->target) {
        uint64_t diff = (uint64_t)((int64_t)t->target - t->counter);

        cmp_increments = (diff + step - 1) / step;

        if (!cmp_increments)
            cmp_increments = 1;
    }

    if ((uint64_t)t->counter <= ovf) {
        uint64_t diff = ovf - (uint64_t)t->counter;

        ovf_increments = (diff / step) + 1;
    }

    t->cycles_until_check = (cmp_increments < ovf_increments) ? cmp_increments : ovf_increments;

    if (!t->cycles_until_check)
        t->cycles_until_check = 1;
}

static inline uint32_t iop_timer_cycles_until_check(struct iop_timer* t) {
    if (!t->check_enabled)
        return 0xffffffffu;

    if (!t->delta_reload)
        return 0xffffffffu;

    if (!t->cycles_until_check)
        return 1;

    uint64_t cycles = t->delta;

    if (t->cycles_until_check > 1) {
        cycles += (t->cycles_until_check - 1) * t->delta_reload;
    }

    if (!cycles)
        cycles = 1;

    if (cycles > 0x7fffffffu)
        cycles = 0x7fffffffu;

    return (uint32_t)cycles;
}

static inline uint64_t iop_timer_abs_check_cycle(struct iop_timer* t) {
    return t->last_sync_cycle + iop_timer_cycles_until_check(t);
}

static inline void iop_timer_fire_irq(struct ps2_iop_timers* timers, struct iop_timer* t, int i) {
    ps2_iop_intc_irq(timers->intc, timer_get_irq_mask(i));

    if (!t->rep_irq) {
        t->irq_en = 0;
    } else if (t->levl) {
        t->irq_en = !t->irq_en;
    }
}

static inline void iop_timer_advance_counter(struct ps2_iop_timers* timers, struct iop_timer* t, int i, uint64_t increments) {
    if (!increments)
        return;

    while (increments) {
        if (t->cycles_until_check > 1) {
            uint64_t skip = t->cycles_until_check - 1;

            if (skip > increments)
                skip = increments;

            t->counter += (int64_t)(skip * t->step);
            t->cycles_until_check -= skip;
            increments -= skip;

            if (!increments)
                break;
        }

        uint32_t prev = (uint32_t)t->counter;
        t->counter += t->step;

        if (t->counter >= (int64_t)t->target && prev < t->target) {
            t->cmp_irq_set = 1;

            if (t->cmp_irq && t->irq_en) {
                iop_timer_fire_irq(timers, t, i);

                if (t->irq_reset) {
                    t->counter = 0;
                    t->delta = t->delta_reload;
                    iop_timer_update_internal_from_delta(t);
                }
            }
        }

        uint64_t ovf = iop_timer_ovf_limit(i);

        if ((uint64_t)t->counter > ovf) {
            t->ovf_irq_set = 1;

            if (t->ovf_irq && t->irq_en) {
                iop_timer_fire_irq(timers, t, i);

                if (t->irq_reset) {
                    t->counter = 0;
                    t->delta = t->delta_reload;
                    iop_timer_update_internal_from_delta(t);
                }
            }

            t->counter -= (int64_t)(ovf + 1);
        }

        iop_timer_update_event(t, i);
        increments--;
    }
}

static inline void iop_timer_sync(struct ps2_iop_timers* timers, struct iop_timer* t, int i) {
    uint64_t elapsed = timers->current_cycle - t->last_sync_cycle;

    if (!elapsed)
        return;

    t->last_sync_cycle = timers->current_cycle;

    if (elapsed < t->delta) {
        t->delta -= (uint32_t)elapsed;
        iop_timer_update_internal_from_delta(t);
        return;
    }

    elapsed -= t->delta;

    uint64_t increments = 1;
    uint32_t period = t->delta_reload;

    if (period) {
        increments += elapsed / period;

        uint32_t rem = (uint32_t)(elapsed % period);
        t->delta = period - rem;

        if (!t->delta)
            t->delta = period;
    } else {
        t->delta = 0;
    }

    iop_timer_update_internal_from_delta(t);
    iop_timer_advance_counter(timers, t, i, increments);
}

static void iop_timers_recompute_next_check(struct ps2_iop_timers* timers) {
    uint64_t min_cycle = UINT64_MAX;

    for (int i = 0; i < 6; i++) {
        if (!(timers->active_mask & (1u << i)))
            continue;

        struct iop_timer* t = &timers->timer[i];
        uint64_t next = iop_timer_abs_check_cycle(t);

        if (next < min_cycle)
            min_cycle = next;
    }

    timers->next_check_cycle = min_cycle;
}

static inline void iop_timer_sync_by_index(struct ps2_iop_timers* timers, int i) {
    iop_timer_sync(timers, &timers->timer[i], i);
}

void ps2_iop_timers_tick(struct ps2_iop_timers* timers) {
    ps2_iop_timers_tick_cycles(timers, 1);
}

void ps2_iop_timers_tick_cycles(struct ps2_iop_timers* timers, uint32_t cycles) {
    if (!cycles || !timers->active_mask)
        return;

    timers->current_cycle += cycles;

    if (timers->current_cycle < timers->next_check_cycle)
        return;

    for (int i = 0; i < 6; i++) {
        if (timers->active_mask & (1u << i))
            iop_timer_sync(timers, &timers->timer[i], i);
    }

    iop_timers_recompute_next_check(timers);
}

uint32_t iop_timer_handle_mode_read(struct ps2_iop_timers* timers, int i) {
    iop_timer_sync_by_index(timers, i);

    uint32_t r = timers->timer[i].mode;

    timers->timer[i].cmp_irq_set = 0;
    timers->timer[i].ovf_irq_set = 0;
    // timers->timer[i].irq_en = 1;

    // if (i == 5)
    // printf("iop: Timer %d mode read %08x -> %08x\n", i, r, timers->timer[i].mode);

    return r;
}

uint64_t ps2_iop_timers_read32(struct ps2_iop_timers* timers, uint32_t addr) {
    switch (addr & 0xfff) {
        case 0x100: iop_timer_sync_by_index(timers, 0); return timers->timer[0].counter;
        case 0x110: iop_timer_sync_by_index(timers, 1); return timers->timer[1].counter;
        case 0x120: iop_timer_sync_by_index(timers, 2); return timers->timer[2].counter;
        case 0x480: iop_timer_sync_by_index(timers, 3); return timers->timer[3].counter;
        case 0x490: iop_timer_sync_by_index(timers, 4); return timers->timer[4].counter;
        case 0x4a0: iop_timer_sync_by_index(timers, 5); return timers->timer[5].counter;
        case 0x108: iop_timer_sync_by_index(timers, 0); return timers->timer[0].target;
        case 0x118: iop_timer_sync_by_index(timers, 1); return timers->timer[1].target;
        case 0x128: iop_timer_sync_by_index(timers, 2); return timers->timer[2].target;
        case 0x488: iop_timer_sync_by_index(timers, 3); return timers->timer[3].target;
        case 0x498: iop_timer_sync_by_index(timers, 4); return timers->timer[4].target;
        case 0x4a8: iop_timer_sync_by_index(timers, 5); return timers->timer[5].target;
        case 0x104: return iop_timer_handle_mode_read(timers, 0);
        case 0x114: return iop_timer_handle_mode_read(timers, 1);
        case 0x124: return iop_timer_handle_mode_read(timers, 2);
        case 0x484: return iop_timer_handle_mode_read(timers, 3);
        case 0x494: return iop_timer_handle_mode_read(timers, 4);
        case 0x4a4: return iop_timer_handle_mode_read(timers, 5);
    }

    return 0;
}

void iop_timer_handle_mode_write(struct ps2_iop_timers* timers, int t, uint64_t data) {
    struct iop_timer* timer = &timers->timer[t];

    iop_timer_sync_by_index(timers, t);

    timer->counter = 0;
    timer->internal = 0;
    timer->mode |= 0x400;
    timer->mode &= 0x1c00;
    timer->mode |= data & 0xe3ff;

    iop_timer_refresh_clock(timer, t);
    timer->delta = timer->delta_reload;
    timer->check_enabled = 1;

    if (!timer->levl) {
        timer->irq_en = 1;
    }

    if (timer->counter >= timer->target) {
        timer->cmp_irq_set = 1;

        // ps2_iop_intc_irq(timers->intc, timer_get_irq_mask(t));
    }

    // printf("iop: Timer %d mode write %08x -> %08x gate_en=%d gate_mode=%d irq_reset=%d cmp_irq=%d ovf_irq=%d rep_irq=%d levl=%d use_ext=%d irq_en=%d t4_prescaler=%d\n",
    //     t, timers->timer[t].mode,
    //     data,
    //     timers->timer[t].gate_en,
    //     timers->timer[t].gate_mode,
    //     timers->timer[t].irq_reset,
    //     timers->timer[t].cmp_irq,
    //     timers->timer[t].ovf_irq,
    //     timers->timer[t].rep_irq,
    //     timers->timer[t].levl,
    //     timers->timer[t].use_ext,
    //     timers->timer[t].irq_en,
    //     timers->timer[t].t4_prescaler
    // );

    timer->last_sync_cycle = timers->current_cycle;

    iop_timer_update_event(timer, t);
    iop_timers_recompute_next_check(timers);
}

void iop_timer_handle_target_write(struct ps2_iop_timers* timers, int t, uint64_t data) {
    struct iop_timer* timer = &timers->timer[t];

    iop_timer_sync_by_index(timers, t);

    timer->target = data;

    if (!timer->levl) {
        timer->irq_en = 1;
    }

    iop_timer_update_event(timer, t);
    iop_timers_recompute_next_check(timers);

    if (t != 5)
        return;

    // printf("iop: Timer %d target write %08x levl=%d mode=%08x counter=%08x\n", t, data, timer->levl, timer->mode, timer->counter);
}

void ps2_iop_timers_write32(struct ps2_iop_timers* timers, uint32_t addr, uint64_t data) {
    switch (addr & 0xfff) {
        case 0x100:
            iop_timer_sync_by_index(timers, 0);
            timers->timer[0].counter = data;
            iop_timer_update_event(&timers->timer[0], 0);
            iop_timers_recompute_next_check(timers);
            break;
        case 0x110:
            iop_timer_sync_by_index(timers, 1);
            timers->timer[1].counter = data;
            iop_timer_update_event(&timers->timer[1], 1);
            iop_timers_recompute_next_check(timers);
            break;
        case 0x120:
            iop_timer_sync_by_index(timers, 2);
            timers->timer[2].counter = data;
            iop_timer_update_event(&timers->timer[2], 2);
            iop_timers_recompute_next_check(timers);
            break;
        case 0x480:
            iop_timer_sync_by_index(timers, 3);
            timers->timer[3].counter = data;
            iop_timer_update_event(&timers->timer[3], 3);
            iop_timers_recompute_next_check(timers);
            break;
        case 0x490:
            iop_timer_sync_by_index(timers, 4);
            timers->timer[4].counter = data;
            iop_timer_update_event(&timers->timer[4], 4);
            iop_timers_recompute_next_check(timers);
            break;
        case 0x4a0:
            iop_timer_sync_by_index(timers, 5);
            timers->timer[5].counter = data;
            iop_timer_update_event(&timers->timer[5], 5);
            iop_timers_recompute_next_check(timers);
            break;
        case 0x108: iop_timer_handle_target_write(timers, 0, data); break;
        case 0x118: iop_timer_handle_target_write(timers, 1, data); break;
        case 0x128: iop_timer_handle_target_write(timers, 2, data); break;
        case 0x488: iop_timer_handle_target_write(timers, 3, data); break;
        case 0x498: iop_timer_handle_target_write(timers, 4, data); break;
        case 0x4a8: iop_timer_handle_target_write(timers, 5, data); break;
        case 0x104: iop_timer_handle_mode_write(timers, 0, data); break;
        case 0x114: iop_timer_handle_mode_write(timers, 1, data); break;
        case 0x124: iop_timer_handle_mode_write(timers, 2, data); break;
        case 0x484: iop_timer_handle_mode_write(timers, 3, data); break;
        case 0x494: iop_timer_handle_mode_write(timers, 4, data); break;
        case 0x4a4: iop_timer_handle_mode_write(timers, 5, data); break;
    }
}