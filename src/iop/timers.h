#ifndef IOP_TIMER_H
#define IOP_TIMER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

struct iop_timer {
    uint64_t counter;

    union {
        uint32_t mode;

        struct {
            unsigned int gate_en : 1;
            unsigned int gate_mode : 2;
            unsigned int irq_reset : 1;
            unsigned int cmp_irq : 1;
            unsigned int ovf_irq : 1;
            unsigned int rep_irq : 1;
            unsigned int levl : 1;
            unsigned int use_ext : 1;
            unsigned int t2_prescaler : 1;
            unsigned int irq_en : 1;
            unsigned int cmp_irq_set : 1;
            unsigned int ovf_irq_set : 1;
            unsigned int t4_prescaler : 1;
            unsigned int t5_prescaler : 1;
            unsigned int unused : 17;
        };
    };

    uint32_t target;
};

struct ps2_iop_timers {
    struct iop_timer timer[6];

    struct ps2_iop_intc* intc;
    struct sched_state* sched;
};

struct ps2_iop_timers* ps2_iop_timers_create(void);
void ps2_iop_timers_init(struct ps2_iop_timers* timers, struct ps2_iop_intc* intc, struct sched_state* sched);
void ps2_iop_timers_destroy(struct ps2_iop_timers* timers);
void ps2_iop_timers_tick(struct ps2_iop_timers* timers);
uint64_t ps2_iop_timers_read32(struct ps2_iop_timers* timers, uint32_t addr);
void ps2_iop_timers_write32(struct ps2_iop_timers* timers, uint32_t addr, uint64_t data);

#ifdef __cplusplus
}
#endif

#endif