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

namespace iris::ee::timers {

struct Timer {
    uint32_t counter;
    uint16_t mode;
    uint32_t compare;
    uint16_t hold;
    
    // Internal state
    int id;
    uint32_t internal;
    uint32_t delta;
    uint32_t delta_reload;
    uint32_t check_reload;
    int cycles_until_compare;
    int cycles_until_overflow;
    int cycles_until_check;
    int check_enabled;
    
    // Lazy evaluation
    uint64_t last_sync_cycle;
    
    // Mode fields
    int clks;
    int gate;
    int gats;
    int gatm;
    int zret;
    int cue;
    int cmpe;
    int ovfe;
};

struct Timers {
    struct {
        ee::intc::Intc* intc;
        scheduler::Scheduler* sched;
    } hw;

    Timer timer[4];
    uint8_t active_mask;
    uint8_t ee_cycle_phase;
    
    // Global cycle tracking for lazy evaluation
    uint64_t current_cycle;
    uint64_t scheduler_advanced_cycles;
    int irq_event_pending;

    logger::Logger* logger = nullptr;
    size_t logger_id = 0;
};

Timers* create(logger::Logger* logger, scheduler::Scheduler* sched);
void connect(Timers* timers, ee::intc::Intc* intc);
void reset(Timers* timers);
void destroy(Timers* timers);
uint64_t read16(Timers* timers, uint32_t addr);
uint64_t read32(Timers* timers, uint32_t addr);
void write32(Timers* timers, uint32_t addr, uint64_t data);
void write16(Timers* timers, uint32_t addr, uint64_t data);
void tick(Timers* timers);
void tick_cycles(Timers* timers, uint32_t cycles);
void handle_hblank(Timers* timers);
void handle_vblank_in(Timers* timers);
void handle_vblank_out(Timers* timers);

}
