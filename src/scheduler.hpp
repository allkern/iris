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

#include <cstdint>
#include <vector>

#include "logger.hpp"

namespace iris::scheduler {

typedef void (*EventCallback)(void* udata, int overshoot);

inline constexpr int64_t NO_EVENT = INT64_MAX;

struct Event {
    EventCallback callback;
    int64_t cycles;
    const char* name;
    void* udata;
};

struct Scheduler {
    struct Entry {
        EventCallback callback;
        const char* name;
        int64_t deadline;
        void* udata;
    };

    std::vector <Entry> events;

    int64_t now = 0;

    logger::Logger* logger = nullptr;
    size_t logger_id = 0;
};

Scheduler* create(logger::Logger* logger);
void reset(Scheduler* sched);
void schedule(Scheduler* sched, const Event& event);
int tick(Scheduler* sched, int64_t cycles);
int64_t cycles_to_next(const Scheduler* sched);
void destroy(Scheduler* sched);

}
