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

#include <algorithm>

#include "scheduler.hpp"

namespace iris::scheduler {

Scheduler* create(logger::Logger* logger) {
    Scheduler* sched = new Scheduler();

    sched->logger = logger;
    sched->logger_id = logger::register_source(logger, "sched");

    sched->events.reserve(32);

    return sched;
}

void reset(Scheduler* sched) {
    sched->events.clear();

    sched->now = 0;
}

static bool fires_later_than(const Scheduler::Entry& entry, int64_t deadline) {
    return entry.deadline > deadline;
}

void schedule(Scheduler* sched, const Event& event) {
    Scheduler::Entry entry = {
        .callback = event.callback,
        .name = event.name,
        .deadline = sched->now + event.cycles,
        .udata = event.udata
    };

    auto pos = std::lower_bound(
        sched->events.begin(),
        sched->events.end(),
        entry.deadline,
        fires_later_than
    );

    sched->events.insert(pos, entry);
}

int tick(Scheduler* sched, int64_t cycles) {
    sched->now += cycles;

    if (sched->events.empty())
        return 0;

    if (sched->events.back().deadline > sched->now)
        return 0;

    Scheduler::Entry entry = sched->events.back();

    sched->events.pop_back();

    entry.callback(entry.udata, (int)(entry.deadline - sched->now));

    return 1;
}

int64_t cycles_to_next(const Scheduler* sched) {
    if (sched->events.empty())
        return NO_EVENT;

    return sched->events.back().deadline - sched->now;
}

void destroy(Scheduler* sched) {
    delete sched;
}

}
