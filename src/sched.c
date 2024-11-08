#include <stdint.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>

#include "sched.h"

struct sched_state* sched_create(void) {
    return malloc(sizeof(struct sched_state));
}

void sched_init(struct sched_state* sched) {
    memset(sched, 0, sizeof(struct sched_state));

    sched->nevents = 0;
    sched->cap = 0;
    sched->events = NULL;
    sched->offset = 0;
}

int event_compare(const void* a, const void* b) {
    return ((struct sched_event*)a)->cycles - ((struct sched_event*)b)->cycles;
}

void sched_schedule(struct sched_state* sched, struct sched_event event) {
    printf("sched: Schedule \'%s\'\n", event.name);

    if (!sched->nevents) {
        sched->events = malloc(sizeof(struct sched_event) * 32);
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

        sched->nevents = 2;
    } else {
        if (sched->nevents == sched->cap) {
            sched->cap <<= 1;
            sched->events = realloc(sched->events, sizeof(struct sched_event) * sched->cap);
        }

        for (int i = 0; i < sched->nevents - 1; i++) {
            sched->events[sched->nevents - i] = sched->events[sched->nevents - i - 1];
        }

        ++sched->nevents;

        sched->events[0] = event;

        if (event.cycles > sched->events[1].cycles) {
            qsort(sched->events, sched->nevents, sizeof(struct sched_event), event_compare);
        }
    }
}

void sched_tick(struct sched_state* sched, int cycles) {
    // printf("sched->nevents=%d\n", sched->nevents);

    // exit(1);

    if (!sched->nevents)
        return;

    sched->events[0].cycles -= cycles;
    sched->offset += cycles;

    if (sched->events[0].cycles > 0)
        return;

    --sched->nevents;

    for (int i = 0; i < sched->nevents; i++) {
        sched->events[i] = sched->events[i + 1];
        sched->events[i].cycles -= sched->offset;
    }

    // Provide callback with overshot cycles
    sched->events[0].callback(sched->events[0].udata, sched->events[0].cycles);

    sched->offset = 0;
}

const struct sched_event* sched_next_event(struct sched_state* sched) {
    return &sched->events[0];
}

void sched_destroy(struct sched_state* sched) {
    free(sched->events);
    free(sched);
}