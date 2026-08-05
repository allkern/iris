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

void schedule(Scheduler* sched, const Event& event) {
    Scheduler::Entry entry = {
        .callback = event.callback,
        .name = event.name,
        .deadline = sched->now + event.cycles,
        .udata = event.udata
    };

    // upper_bound, not lower_bound: ties fire in scheduling order.
    auto pos = std::upper_bound(
        sched->events.begin(),
        sched->events.end(),
        entry.deadline,
        [](int64_t deadline, const Scheduler::Entry& e) {
            return deadline < e.deadline;
        }
    );

    sched->events.insert(pos, entry);
}

int tick(Scheduler* sched, int64_t cycles) {
    sched->now += cycles;

    if (sched->events.empty())
        return 0;

    if (sched->events.front().deadline > sched->now)
        return 0;

    // Copy and erase before dispatching. The callback may reschedule,
    // which can reallocate the vector.
    Scheduler::Entry entry = sched->events.front();

    sched->events.erase(sched->events.begin());

    entry.callback(entry.udata, (int)(entry.deadline - sched->now));

    return 1;
}

int64_t cycles_to_next(const Scheduler* sched) {
    if (sched->events.empty())
        return NO_EVENT;

    return sched->events.front().deadline - sched->now;
}

void destroy(Scheduler* sched) {
    delete sched;
}

}
