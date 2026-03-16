#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "ioboard.h"

struct s14x_ioboard* s14x_ioboard_create(void) {
    return malloc(sizeof(struct s14x_ioboard));
}

int s14x_ioboard_init(struct s14x_ioboard* ioboard, int mode) {
    memset(ioboard, 0, sizeof(struct s14x_ioboard));

    ioboard->version = 0x0104;
    ioboard->switches = 0xffff;
    ioboard->mode = mode;
}

void s14x_ioboard_destroy(struct s14x_ioboard* ioboard) {
    free(ioboard);
}

uint8_t link_calculate_checksum(struct link_packet* packet) {
    uint8_t checksum = 0;

    for (int i = 0; i < sizeof(packet->data); i++) {
        checksum += packet->data[i];
    }

    return checksum;
}

void s14x_ioboard_handle_packet(void* udata, struct link_packet* in, struct link_packet* out) {
    struct s14x_ioboard* ioboard = (struct s14x_ioboard*)udata;

    // Setup header
    *out = *in;

    out->src_node = in->dst_node;
    out->dst_node = in->src_node;

    switch (in->cmd) {
        // GetPCBInformation
        case 0x0f: {
            out->data[0] = 0;
            out->data[1] = 0;
            out->data[2] = ioboard->version >> 8;
            out->data[3] = ioboard->version & 0xff;
        } break;

        // GetSystemSwitches
        case 0x10: {
            out->data[0] = ioboard->switches & 0xff;
            out->data[1] = ioboard->switches >> 8;
        } break;

        case 0x0d: // ?
        case 0x11: // ?
        case 0x18: // Switch
        case 0x38: // SCI
        case 0x39: // GetSwitches
        case 0x48: // CoinSensor
        case 0x58: // MechanicalSensor
        case 0xa5: // Reset
        default: {
            out->data[0] = in->data[0];
        } break;
    }

    out->checksum = link_calculate_checksum(out);
}

void s14x_ioboard_press_switch(struct s14x_ioboard* ioboard, uint16_t mask) {
    ioboard->switches &= ~mask;
}

void s14x_ioboard_release_switch(struct s14x_ioboard* ioboard, uint16_t mask) {
    ioboard->switches |= mask;
}