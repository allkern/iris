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

void iop_timer_tick(struct ps2_iop_timers* timers, int i) {
    struct iop_timer* t = &timers->timer[i];

    uint32_t prev = t->counter;

    // To-do: Breaks Crazy Taxi (USA)
    if (i == 1) {
        if (t->use_ext) {
            if (t->internal != 90) {
                ++t->internal;
            } else {
                ++t->counter;
                t->internal = 0;
            }
        } else {
            ++t->counter;
        }
    } else {
        t->counter += 9;
    }

    if (t->counter >= t->target && prev < t->target) {
        // printf("iop: Timer 5 reached target %08x <= %08x\n", t->counter, t->target);

        t->cmp_irq_set = 1;

        if (t->cmp_irq && t->irq_en) {
            // printf(" timer %d irq\n", i);

            ps2_iop_intc_irq(timers->intc, timer_get_irq_mask(i));

            if (!t->rep_irq) {
                t->irq_en = 0;
            } else {
                if (t->levl) {
                    t->irq_en = !t->irq_en;
                }
            }

            if (t->irq_reset) {
                t->counter = 0;
            }

            // printf("iop: Timer %d compare IRQ cnt=%d tgt=%d rep=%d levl=%d irq_en=%d irq_reset=%d\n", i,
            //     t->counter,
            //     t->target,
            //     t->rep_irq,
            //     t->levl,
            //     t->irq_en,
            //     t->irq_reset
            // );
        }
    }

    uint64_t ovf = (i < 3) ? 0xffff : 0xffffffff;

    if (t->counter > ovf) {
        t->ovf_irq_set = 1;

        if (t->ovf_irq && t->irq_en) {
            // printf("iop: Timer %d overflow IRQ rep=%d levl=%d irq_en=%d irq_reset=%d\n", i,
            //     t->rep_irq,
            //     t->levl,
            //     t->irq_en,
            //     t->irq_reset
            // );

            ps2_iop_intc_irq(timers->intc, timer_get_irq_mask(i));

            if (!t->rep_irq) {
                t->irq_en = 0;
            } else {
                if (t->levl) {
                    t->irq_en = !t->irq_en;
                }
            }

            if (t->irq_reset) {
                t->counter = 0;
            }
        }

        t->counter -= ovf;
    }
}

void ps2_iop_timers_tick(struct ps2_iop_timers* timers) {
    for (int i = 0; i < 6; i++) {
        iop_timer_tick(timers, i);
    }
}

uint32_t iop_timer_handle_mode_read(struct ps2_iop_timers* timers, int i) {
    uint32_t r = timers->timer[i].mode;

    timers->timer[i].cmp_irq_set = 0;
    timers->timer[i].ovf_irq_set = 0;
    timers->timer[i].irq_en = 1;

    // if (i == 5)
    // printf("iop: Timer %d mode read %08x -> %08x\n", i, r, timers->timer[i].mode);

    return r;
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

    timer->counter = 0;
    timer->mode |= 0x400;
    timer->mode &= 0x1c00;
    timer->mode |= data & 0xe3ff;

    if (timer->counter >= timer->target) {
        timer->cmp_irq_set = 1;

        // ps2_iop_intc_irq(timers->intc, timer_get_irq_mask(t));
    }

    printf("iop: Timer %d mode write %08x -> %08x gate_en=%d gate_mode=%d irq_reset=%d cmp_irq=%d ovf_irq=%d rep_irq=%d levl=%d use_ext=%d irq_en=%d t4_prescaler=%d\n",
        t, timers->timer[t].mode,
        data,
        timers->timer[t].gate_en,
        timers->timer[t].gate_mode,
        timers->timer[t].irq_reset,
        timers->timer[t].cmp_irq,
        timers->timer[t].ovf_irq,
        timers->timer[t].rep_irq,
        timers->timer[t].levl,
        timers->timer[t].use_ext,
        timers->timer[t].irq_en,
        timers->timer[t].t4_prescaler
    );

    /* To-do: Schedule timer interrupts for better performance */
}

void iop_timer_handle_target_write(struct ps2_iop_timers* timers, int t, uint64_t data) {
    struct iop_timer* timer = &timers->timer[t];

    timer->target = data;

    if (!timer->levl) {
        timer->irq_en = 1;
    }

    if (t != 5)
        return;

    // printf("iop: Timer %d target write %08x levl=%d mode=%08x counter=%08x\n", t, data, timer->levl, timer->mode, timer->counter);
}

void ps2_iop_timers_write32(struct ps2_iop_timers* timers, uint32_t addr, uint64_t data) {
    switch (addr & 0xfff) {
        case 0x100: /* printf("iop: Timer 0 counter write %08x prev=%08x\n", data, timers->timer[0].counter); */ timers->timer[0].counter = data; break;
        case 0x110: /* printf("iop: Timer 1 counter write %08x prev=%08x\n", data, timers->timer[1].counter); */ timers->timer[1].counter = data; break;
        case 0x120: /* printf("iop: Timer 2 counter write %08x prev=%08x\n", data, timers->timer[2].counter); */ timers->timer[2].counter = data; break;
        case 0x480: /* printf("iop: Timer 3 counter write %08x prev=%08x\n", data, timers->timer[3].counter); */ timers->timer[3].counter = data; break;
        case 0x490: /* printf("iop: Timer 4 counter write %08x prev=%08x\n", data, timers->timer[4].counter); */ timers->timer[4].counter = data; break;
        case 0x4a0: /* printf("iop: Timer 5 counter write %08x prev=%08x\n", data, timers->timer[5].counter); */ timers->timer[5].counter = data; break;
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