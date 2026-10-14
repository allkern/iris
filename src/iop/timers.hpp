/*
    This file is part of the Iris PlayStation 2 emulator project.
    Hosted at https://github.com/allkern/iris

    MIT License

    Copyright (c) 2026 Allkern/Lycoder (Lisandro Alarcón)

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in all
    copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
    SOFTWARE.
*/

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
