#include <stdint.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>

#include "scheduler.h"

struct sched_state* sched_create(void) {
    return malloc(sizeof(struct sched_state));
}

void sched_init(struct sched_state* sched) {
    memset(sched, 0, sizeof(struct sched_state));

    if (sched->events)
        free(sched->events);

    sched->nevents = 0;
    sched->cap = 0;
    sched->events = NULL;
    sched->offset = 0;
}

// Min-heap helper: bubble up from position i to root
static inline void sched_bubble_up(struct sched_state* sched, int i) {
    while (i > 0) {
        int parent = (i - 1) >> 1;

        if (sched->events[i].cycles >= sched->events[parent].cycles)
            break;

        // Swap
        struct sched_event tmp = sched->events[i];
        sched->events[i] = sched->events[parent];
        sched->events[parent] = tmp;

        i = parent;
    }
}

// Min-heap helper: bubble down from position i
static inline void sched_bubble_down(struct sched_state* sched, int i) {
    while (1) {
        int smallest = i;
        int left = (i << 1) + 1;
        int right = (i << 1) + 2;

        if (left < sched->nevents && sched->events[left].cycles < sched->events[smallest].cycles)
            smallest = left;

        if (right < sched->nevents && sched->events[right].cycles < sched->events[smallest].cycles)
            smallest = right;

        if (smallest == i)
            break;

        // Swap
        struct sched_event tmp = sched->events[i];
        sched->events[i] = sched->events[smallest];
        sched->events[smallest] = tmp;

        i = smallest;
    }
}

void sched_schedule(struct sched_state* sched, struct sched_event event) {
    // Ensure capacity
    if (sched->nevents == sched->cap) {
        int new_cap = (sched->cap == 0) ? 32 : sched->cap << 1;
        struct sched_event* new_events = realloc(sched->events, sizeof(struct sched_event) * new_cap);

        if (!new_events) {
            printf("sched: Failed to allocate new event\n");
            exit(1);
        }

        sched->events = new_events;
        sched->cap = new_cap;
    }

    // Sync all events on insertion to handle offset
    if (sched->nevents > 0 && sched->offset > 0) {
        for (int i = 0; i < sched->nevents; i++) {
            sched->events[i].cycles -= sched->offset;
        }
        sched->offset = 0;
    }

    // Insert at end and bubble up
    sched->events[sched->nevents] = event;
    sched_bubble_up(sched, sched->nevents);
    sched->nevents++;
}

int sched_tick(struct sched_state* sched, int cycles) {
    if (!sched->nevents)
        return 0;

    // Decrease the minimum element's cycle count
    sched->events[0].cycles -= cycles;
    sched->offset += cycles;

    // If minimum hasn't triggered yet, we're done
    if (sched->events[0].cycles > 0)
        return 0;

    // Extract the minimum event
    struct sched_event event = sched->events[0];

    // Move last event to root and bubble down
    sched->nevents--;
    if (sched->nevents > 0) {
        sched->events[0] = sched->events[sched->nevents];
        sched_bubble_down(sched, 0);

        // Apply offset to remaining events
        for (int i = 0; i < sched->nevents; i++) {
            sched->events[i].cycles -= sched->offset;
        }
    }

    sched->offset = 0;

    // Provide callback with overshot cycles
    event.callback(event.udata, event.cycles);

    return 1;
}

const struct sched_event* sched_next_event(struct sched_state* sched) {
    return &sched->events[0];
}

void sched_reset(struct sched_state* sched) {
    sched->nevents = 0;
    sched->offset = 0;
}

void sched_destroy(struct sched_state* sched) {
    free(sched->events);
    free(sched);
}