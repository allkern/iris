#ifndef SCHED_H
#define SCHED_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

struct sched_event {
    long cycles;
    void (*callback)(void*, int);
    const char* name;
    void* udata;
};

struct sched_state {
    struct sched_event* events;
    int nevents;
    int cap;
    uint64_t offset;
};

struct sched_state* sched_create(void);
void sched_init(struct sched_state* sched);
void sched_schedule(struct sched_state* sched, struct sched_event event);
void sched_reset(struct sched_state* sched);
int sched_tick(struct sched_state* sched, int cycles);
const struct sched_event* sched_next_event(struct sched_state* sched);
void sched_destroy(struct sched_state* sched);

#ifdef __cplusplus
}
#endif

#endif