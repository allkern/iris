#ifndef IOP_TIMER_H
#define IOP_TIMER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

struct iop_timer {
    int64_t counter;

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
    int64_t internal;

    // Lazy synchronization state
    uint64_t last_sync_cycle;
    uint32_t delta;
    uint32_t delta_reload;
    uint64_t cycles_until_check;
    uint8_t step;
    uint8_t check_enabled;
};

struct ps2_iop_timers {
    struct iop_timer timer[6];

    uint8_t active_mask;
    uint64_t current_cycle;
    uint64_t next_check_cycle;
    uint64_t scheduler_advanced_cycles;
    int irq_event_pending;

    struct ps2_iop_intc* intc;
    struct sched_state* sched;
};

struct ps2_iop_timers* ps2_iop_timers_create(void);
void ps2_iop_timers_init(struct ps2_iop_timers* timers, struct ps2_iop_intc* intc, struct sched_state* sched);
void ps2_iop_timers_destroy(struct ps2_iop_timers* timers);
void ps2_iop_timers_tick(struct ps2_iop_timers* timers);
void ps2_iop_timers_tick_cycles(struct ps2_iop_timers* timers, uint32_t cycles);
uint64_t ps2_iop_timers_read32(struct ps2_iop_timers* timers, uint32_t addr);
void ps2_iop_timers_write32(struct ps2_iop_timers* timers, uint32_t addr, uint64_t data);

#ifdef __cplusplus
}
#endif

#endif