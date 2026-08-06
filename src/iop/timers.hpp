#pragma once

#include "scheduler.hpp"
#include "intc.hpp"
#include "logger.hpp"

namespace iris::iop::timers {

struct Timer {
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

struct Timers {
    struct {
        intc::Intc* intc;
        scheduler::Scheduler* sched;
    } hw;

    Timer timer[6];

    uint8_t active_mask;
    uint64_t current_cycle;
    uint64_t next_check_cycle;
    uint64_t scheduler_advanced_cycles;
    int irq_event_pending;


    logger::Logger* logger = nullptr;
    size_t logger_id = 0;
};

Timers* create(logger::Logger* logger, intc::Intc* intc, scheduler::Scheduler* sched);
void reset(Timers* timers);
void destroy(Timers* timers);
void tick(Timers* timers);
void tick_cycles(Timers* timers, uint32_t cycles);
uint64_t read32(Timers* timers, uint32_t addr);
void write32(Timers* timers, uint32_t addr, uint64_t data);

}
