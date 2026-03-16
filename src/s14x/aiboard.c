#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "aiboard.h"

struct s14x_aiboard* s14x_aiboard_create(void) {
    return malloc(sizeof(struct s14x_aiboard));
}

int s14x_aiboard_init(struct s14x_aiboard* aiboard) {
    memset(aiboard, 0, sizeof(struct s14x_aiboard));

    aiboard->version = 0x0104;
}

void s14x_aiboard_destroy(struct s14x_aiboard* aiboard) {
    free(aiboard);
}

void s14x_aiboard_handle_packet(void* udata, struct link_packet* in, struct link_packet* out) {
    struct s14x_aiboard* aiboard = (struct s14x_aiboard*)udata;

    // if (cp == 0x38) {
    //     link->ram[addr+0] = node;
    //     link->ram[addr+1] = 0; // Broadcast
    //     link->ram[addr+2] = 0x38;
    //     link->ram[addr+3] = 0;
    //     link->ram[addr+0x38] = 0x20;
    //     // link->ram[addr+0x39] = link->ram[0x79];
    //     // link->ram[addr+0x3a] = link->ram[0x7a];
    //     // link->ram[addr+0x3b] = link->ram[0x7b];
    //     // link->ram[addr+0x3c] = link->ram[0x7c];
    //     // link->ram[addr+0x3d] = link->ram[0x7d];
    //     // link->ram[addr+0x3e] = link->ram[0x7e];
    //     link->ram[addr+0x3f] = 0;

    //     // for (int i = 0; i < 7; i++)
    //     //     link->ram[addr+0x3f] += link->ram[addr+0x38+i];

    //     ps2_iop_intc_irq(link->intc, IOP_INTC_DEV9);

    //     return;
    // }
}