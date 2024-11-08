#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "timers.h"
#include "intc.h"
#include "sched.h"

struct ps2_iop_timers* ps2_iop_timers_create(void) {
    return malloc(sizeof(struct ps2_iop_timers));
}

void ps2_iop_timers_init(struct ps2_iop_timers* timers, struct ps2_iop_intc* intc, struct sched_state* sched) {
    memset(timers, 0, sizeof(struct ps2_iop_timers));

    timers->intc = intc;
    timers->sched = sched;
}

void ps2_iop_timers_destroy(struct ps2_iop_timers* timers) {
    free(timers);
}

void iop_timer_tick(struct ps2_iop_timers* timers, struct iop_timer* timer) {
    uint32_t prev = timer->counter;

    ++timer->counter;

    if (timer->counter >= timer->target && prev < timer->target) {
        timer->cmp_irq_set = 1;

        if (timer->cmp_irq && timer->irq_en) {
            ps2_iop_intc_irq(timers->intc, IOP_INTC_TIMER5);
        }

        if (timer->irq_reset) {
            timer->counter = 0;
        }
    }

    if (timer->counter > 0xffffffff) {
        timer->ovf_irq_set = 1;

        if (timer->ovf_irq && timer->irq_en) {
            ps2_iop_intc_irq(timers->intc, IOP_INTC_TIMER5);
        }

        timer->counter -= 0xffffffff;
    }
}

void ps2_iop_timers_tick(struct ps2_iop_timers* timers) {
    iop_timer_tick(timers, &timers->timer[5]);
}

uint64_t ps2_iop_timers_read32(struct ps2_iop_timers* timers, uint32_t addr) {
    switch (addr & 0xfff) {
        case 0x100: return timers->timer[0].counter;
        case 0x110: return timers->timer[1].counter;
        case 0x120: return timers->timer[2].counter;
        case 0x480: return timers->timer[3].counter;
        case 0x490: return timers->timer[4].counter;
        case 0x4a0: return timers->timer[5].counter;
        case 0x108: return timers->timer[0].target;
        case 0x118: return timers->timer[1].target;
        case 0x128: return timers->timer[2].target;
        case 0x488: return timers->timer[3].target;
        case 0x498: return timers->timer[4].target;
        case 0x4a8: return timers->timer[5].target;
        case 0x104: timers->timer[0].mode &= 0xe7ff; return timers->timer[0].mode;
        case 0x114: timers->timer[1].mode &= 0xe7ff; return timers->timer[1].mode;
        case 0x124: timers->timer[2].mode &= 0xe7ff; return timers->timer[2].mode;
        case 0x484: timers->timer[3].mode &= 0xe7ff; return timers->timer[3].mode;
        case 0x494: timers->timer[4].mode &= 0xe7ff; return timers->timer[4].mode;
        case 0x4a4: timers->timer[5].mode &= 0xe7ff; return timers->timer[5].mode;
    }

    return 0;
}

void iop_timer_handle_mode_write(struct ps2_iop_timers* timers, int t, uint64_t data) {
    timers->timer[t].counter = 0;
    timers->timer[t].mode |= 0x400;
    timers->timer[t].mode &= 0x1c00;
    timers->timer[t].mode |= data & 0xe3ff;

    /* To-do: Schedule timer interrupts for better performance */
}

void ps2_iop_timers_write32(struct ps2_iop_timers* timers, uint32_t addr, uint64_t data) {
    switch (addr & 0xfff) {
        case 0x100: timers->timer[0].counter = data; break;
        case 0x110: timers->timer[1].counter = data; break;
        case 0x120: timers->timer[2].counter = data; break;
        case 0x480: timers->timer[3].counter = data; break;
        case 0x490: timers->timer[4].counter = data; break;
        case 0x4a0: timers->timer[5].counter = data; break;
        case 0x108: timers->timer[0].target = data; break;
        case 0x118: timers->timer[1].target = data; break;
        case 0x128: timers->timer[2].target = data; break;
        case 0x488: timers->timer[3].target = data; break;
        case 0x498: timers->timer[4].target = data; break;
        case 0x4a8: timers->timer[5].target = data; break;
        case 0x104: iop_timer_handle_mode_write(timers, 0, data); break;
        case 0x114: iop_timer_handle_mode_write(timers, 1, data); break;
        case 0x124: iop_timer_handle_mode_write(timers, 2, data); break;
        case 0x484: iop_timer_handle_mode_write(timers, 3, data); break;
        case 0x494: iop_timer_handle_mode_write(timers, 4, data); break;
        case 0x4a4: iop_timer_handle_mode_write(timers, 5, data); break;
    }
}