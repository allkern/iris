#include <stdint.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>

#include "sched.h"

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

int event_compare(const void* a, const void* b) {
    return ((struct sched_event*)a)->cycles - ((struct sched_event*)b)->cycles;
}

void sched_schedule(struct sched_state* sched, struct sched_event event) {
    if (!sched->nevents) {
        sched->events = realloc(sched->events, sizeof(struct sched_event) * 32);

        if (!sched->events) {
            printf("sched: Failed to allocate new event\n");

            exit(1);
        }

        sched->cap = 32;
        sched->nevents = 1;

        sched->events[0] = event;
    } else if (sched->nevents == 1) {
        if (sched->events[0].cycles > event.cycles) {
            sched->events[1] = sched->events[0];
            sched->events[0] = event;
        } else {
            sched->events[1] = event;
        }

        // Clear offset, if there were more events in the
        // queue, then we should subtract the offset from all of
        // those.
        sched->offset = 0;

        sched->nevents = 2;
    } else {
        if (sched->nevents == sched->cap) {
            sched->cap <<= 1;
            sched->events = realloc(sched->events, sizeof(struct sched_event) * sched->cap);
        }

        // Need to sync events
        // Don't apply offset to the current nearest event (offset is applied by ticking)
        for (int i = 1; i < sched->nevents; i++) {
            sched->events[i].cycles -= sched->offset;
        }

        sched->offset = 0;

        for (int i = 0; i < sched->nevents; i++) {
            sched->events[sched->nevents - i] = sched->events[sched->nevents - i - 1];
        }

        ++sched->nevents;

        sched->events[0] = event;

        if (sched->events[0].cycles > sched->events[1].cycles) {
            qsort(sched->events, sched->nevents, sizeof(struct sched_event), event_compare);
        }
    }
}

int sched_tick(struct sched_state* sched, int cycles) {
    if (!sched->nevents)
        return 0;

    sched->events[0].cycles -= cycles;
    sched->offset += cycles;

    if (sched->events[0].cycles > 0)
        return 0;

    --sched->nevents;

    struct sched_event event = sched->events[0];

    for (int i = 0; i < sched->nevents; i++) {
        sched->events[i] = sched->events[i + 1];
        sched->events[i].cycles -= sched->offset;
    }

    // Provide callback with overshot cycles
    event.callback(event.udata, event.cycles);

    sched->offset = 0;

    return 1;
}

const struct sched_event* sched_next_event(struct sched_state* sched) {
    return &sched->events[0];
}

void sched_destroy(struct sched_state* sched) {
    free(sched->events);
    free(sched);
}