#include "link.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

struct s14x_link* s14x_link_create(void) {
    return malloc(sizeof(struct s14x_link));
}

void s14x_link_init(struct s14x_link* link) {
    memset(link, 0, sizeof(struct s14x_link));
}

uint64_t s14x_link_read(struct s14x_link* link, uint32_t addr) {
    switch (addr) {
        // Watchdog
        case 0x34: return 0;
    }

    printf("s14xlink: Read %08x\n", addr);

    // Implementation for reading from the link
    return 0;
}

void s14x_link_write(struct s14x_link* link, uint32_t addr, uint64_t data) {
    switch (addr) {
        // Watchdog
        case 0x34: return;
    }

    printf("s14xlink: Write %08x %08x\n", addr, data);
    // Implementation for writing to the link
}

void s14x_link_destroy(struct s14x_link* link) {
    free(link);
}