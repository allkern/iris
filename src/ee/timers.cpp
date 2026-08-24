#include <new>

#include "timers.hpp"
#include "scheduler.hpp"

namespace iris::ee::timers {

#define EE_TIMER_SCHED_QUANTUM 64
#define EE_TO_BUSCLK_SHIFT 1

static void ee_timers_schedule_next_irq_event(Timers* timers);

static inline uint64_t ee_timers_ee_cycles_to_busclk(Timers* timers, uint64_t ee_cycles) {
    uint64_t total = ee_cycles + timers->ee_cycle_phase;

    timers->ee_cycle_phase = (uint8_t)(total & ((1u << EE_TO_BUSCLK_SHIFT) - 1));

    return total >> EE_TO_BUSCLK_SHIFT;
}

Timers* create(logger::Logger* logger, scheduler::Scheduler* sched) {
    Timers* timers = new Timers();

    timers->logger = logger;
    timers->logger_id = logger::register_source(logger, "ee_timers");

    timers->hw.sched = sched;

    reset(timers);

    return timers;
}

static inline void ee_timers_update_active_mask(Timers* timers, int t, int cue) {
    uint8_t bit = 1u << t;

    if (cue) {
        timers->active_mask |= bit;
    } else {
        timers->active_mask &= ~bit;
    }
}

void connect(Timers* timers, ee::intc::Intc* intc) {
    timers->hw.intc = intc;
}

void reset(Timers* timers) {
    auto hw = timers->hw;

    logger::Logger* logger = timers->logger;
    size_t logger_id = timers->logger_id;

    new (timers) Timers();

    timers->logger = logger;
    timers->logger_id = logger_id;

    timers->hw = hw;

    for (int i = 0; i < 4; i++)
        timers->timer[i].id = i;
}

static inline void ee_timers_update_event(Timer* t) {
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

    // iris_debug(timers, "timer {}: cycles_until_check={:04x} counter={:04x} compare={:04x} until_compare={:04x} until_overflow={:04x}", //     t, t->cycles_until_check, t->counter, t->compare, cycles_until_compare, cycles_until_overflow
    //);
}

static inline uint32_t ee_timers_cycles_until_check(Timer* t) {
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

static inline void ee_timers_advance_counter(Timers* timers, Timer* t, int i, uint32_t increments) {
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
            iris_fatal_error(timers, "timer {}: error counter={:04x} compare={:04x} cycles_until-check={}", i, counter, t->compare, t->cycles_until_check);
        }

        if (cmp) {
            if (t->cmpe)
                t->mode |= 0x400;

            if (t->zret) {
                counter = 0;
            }

            if (t->cmpe) {
                // iris_debug(timers, "timer {}: compare match counter={:04x} compare={:04x}", i, counter, t->compare);

                ee::intc::irq(timers->hw.intc, ee::intc::TIMER0 + i);
            }

            t->counter = counter;
            ee_timers_update_event(t);
            counter = t->counter;
        }

        if (counter == 0x10000) {
            if (t->ovfe)
                t->mode |= 0x800;

            if (t->ovfe) {
                // iris_debug(timers, "timer {}: overflow match counter={:04x} compare={:04x}", i, counter, t->compare);

                ee::intc::irq(timers->hw.intc, ee::intc::TIMER0 + i);
            }

            t->counter = counter;
            ee_timers_update_event(t);
            counter = t->counter;
        }

        t->counter = counter & 0xffff;
        increments--;
    }
}

static inline void ee_timers_sync_timer(Timers* timers, Timer* t, int i) {
    if (!t->cue) {
        t->last_sync_cycle = timers->current_cycle;
        return;
    }

    // CLKS=3 uses HBLANK as the clock source, so it is stepped by
    // handle_hblank rather than EE/BUSCLK cycle deltas.
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
    Timers* timers = (Timers*)udata;
    uint64_t elapsed_ee_cycles = EE_TIMER_SCHED_QUANTUM;

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

static void ee_timers_schedule_next_irq_event(Timers* timers) {
    if (!timers->hw.sched)
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

    scheduler::Event event;
    event.name = "EE Timer IRQ";
    event.udata = timers;
    event.callback = ee_timers_irq_event_cb;
    event.cycles = (long)sched_cycles;

    scheduler::schedule(timers->hw.sched, event);
    timers->irq_event_pending = 1;
}

void ee_timers_write_counter(Timers* timers, int t, uint32_t data) {
    Timer* timer = &timers->timer[t];

    // Sync to current cycle first
    ee_timers_sync_timer(timers, timer, t);

    // iris_debug(timers, "timer {}: write counter={:04x} data={:04x}", t, timer->counter, data);

    timer->counter = data;
    timer->delta = timer->delta_reload;

    if (timer->check_enabled) {
        ee_timers_update_event(timer);
    }

    ee_timers_schedule_next_irq_event(timers);
}

void ee_timers_write_compare(Timers* timers, int t, uint32_t data) {
    Timer* timer = &timers->timer[t];

    // Sync to current cycle first
    ee_timers_sync_timer(timers, timer, t);

    // iris_debug(timers, "timer {}: write counter={:04x} data={:04x}", t, timer->counter, data);

    if (data < timer->counter) {
        // iris_debug(timers, "timer {}: compare {:04x} >= counter {:04x}", t, data, timer->counter);

        // exit(1);
    } else if (data == timer->counter) {
        // iris_debug(timers, "timer {}: compare {:04x} == counter {:04x}", t, data, timer->counter);

        // exit(1);
    }

    // timer->cycles_until_check = data - timer->counter;
    timer->compare = data;

    if (timer->check_enabled) {
        ee_timers_update_event(timer);
    }

    ee_timers_schedule_next_irq_event(timers);
}

void destroy(Timers* timers) {
    delete timers;
}

static inline void ee_timers_write_mode(Timers* timers, int t, uint32_t data) {
    Timer* timer = &timers->timer[t];

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
    // iris_debug(timers, "Timer {} mode write {:08x} mode={:08x} counter={:04x} compare={:04x} clks={} gate={} gats={} gatm={} zret={} cue={} cmpe={} ovfe={}", //     t, data,
    //     timer->mode,
    //     timer->counter,
    //     timer->compare,
    //     timer->clks, timer->gate, timer->gats, timer->gatm,
    //     timer->zret, timer->cue, timer->cmpe, timer->ovfe
    //);

    if (!timer->cue) {
        timer->check_enabled = 0;
        return;
    }

    // Reset sync point when timer is enabled
    timer->last_sync_cycle = timers->current_cycle;

    if (timer->gate) {
        iris_debug(timers, "Timer {} gate write {:08x}", t, data);

        // exit(1);
    }

    // if (t == 0)
    // iris_debug(timers, "Timer {} mode write {:08x} mode={:08x} counter={:04x} compare={:04x} clks={} gate={} gats={} gatm={} zret={} cue={} cmpe={} ovfe={}", //     t, data,
    //     timer->mode,
    //     timer->counter,
    //     timer->compare,
    //     timer->clks, timer->gate, timer->gats, timer->gatm,
    //     timer->zret, timer->cue, timer->cmpe, timer->ovfe
    //);

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

void tick(Timers* timers) {
    tick_cycles(timers, 1);
}

void tick_cycles(Timers* timers, uint32_t cycles) {
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

void handle_hblank(Timers* timers) {
    if (!timers)
        return;

    for (int i = 0; i < 4; i++) {
        Timer* t = &timers->timer[i];

        if (!t->cue)
            continue;

        if (t->clks != 3)
            continue;

        ee_timers_advance_counter(timers, t, i, 1);
    }
}

void handle_vblank_in(Timers* timers) {
    (void)timers;
}

void handle_vblank_out(Timers* timers) {
    (void)timers;
}

void write16(Timers* timers, uint32_t addr, uint64_t data) {
    int t = (addr >> 11) & 3;

    switch (addr & 0xff) {
        case 0x00: ee_timers_write_counter(timers, t, data & 0xffff); return;
        case 0x10: ee_timers_write_mode(timers, t, data & 0xffff); return;
        case 0x20: ee_timers_write_compare(timers, t, data & 0xffff); return;
        case 0x30: timers->timer[t].hold = data & 0xffff; return;
    }

    iris_fatal_error(timers, "ee: timer {} write {:08x} to {:02x}", t, data, addr & 0xff);
}

void write32(Timers* timers, uint32_t addr, uint64_t data) {
    int t = (addr >> 11) & 3;

    // iris_debug(timers, "ee: timer {} write {:08x} to {:02x}", t, data, addr & 0xff);

    switch (addr & 0xff) {
        case 0x00: ee_timers_write_counter(timers, t, data & 0xffff); return;
        case 0x10: ee_timers_write_mode(timers, t, data & 0xffff); return;
        case 0x20: ee_timers_write_compare(timers, t, data & 0xffff); return;
        case 0x30: timers->timer[t].hold = data & 0xffff; return;
    }
}

uint64_t read16(Timers* timers, uint32_t addr) {
    int t = (addr >> 11) & 3;

    // iris_debug(timers, "ee: timer {} read {:08x}", t, addr & 0xff);

    // Sync timer to current cycle before reading
    ee_timers_sync_timer(timers, &timers->timer[t], t);

    switch (addr & 0xff) {
        case 0x00: return timers->timer[t].counter & 0xffff;
        case 0x10: return timers->timer[t].mode & 0xffff;
        case 0x20: return timers->timer[t].compare & 0xffff;
        case 0x30: return timers->timer[t].hold & 0xffff;
    }

    iris_fatal_error(timers, "ee: timers read16 {:08x}", addr);

    return 0;
}

uint64_t read32(Timers* timers, uint32_t addr) {
    int t = (addr >> 11) & 3;

    // iris_debug(timers, "ee: timer {} read {:08x}", t, addr & 0xff);

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

}
