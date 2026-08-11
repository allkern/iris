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
