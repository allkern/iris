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

void ioboard_process_ic_card(struct link_packet* in, struct link_packet* out) {
    uint8_t channel = in->data[0];
    uint8_t size = in->data[1];
    uint8_t cmd = in->data[5];

    const uint8_t base = 4;

    out->data[base + 0] = 0x02;
    out->data[base + 1] = cmd; // Command ID

    int out_size = 0;

	switch (cmd) {
        case 0x78: {
            out_size = 6;

            out->data[base + 3] = 0x00; // Command result?
            out->data[base + 4] = 0x00;
            out->data[base + 5] = 0xFF;
        } break;

        case 0x7A: {
            out_size = 4;

            out->data[base + 3] = 0x00; // Command result?
        } break;

        case 0x7B: {
            out_size = 6;

            out->data[base + 3] = 0x00; // Command result? (0x03 -> No IC card)
            out->data[base + 4] = 0x00; // ??
            out->data[base + 5] = 0x00; // ??
        } break;

        case 0x80: {
		    out_size = 4;

		    out->data[base + 3] = 0x0D; // Command result?
        } break;

        case 0x9F: {
            out_size = 4;

            out->data[base + 3] = 0x00; // Command result?
        } break;

        case 0xA7: {
            out_size = 4;

            out->data[base + 3] = 0x00; // Command result?
        } break;

        case 0xAC: {
            // Some key stuff (parity)? (game expects 8 bytes in payload)
            out_size = 12;

            out->data[base + 3] = 0x00; // Command result?
            out->data[base + 4] = 0xAA;
            out->data[base + 5] = 0xAA;
            out->data[base + 6] = 0xAA;
            out->data[base + 7] = 0xAA;
            out->data[base + 8] = 0x55;
            out->data[base + 9] = 0x55;
            out->data[base + 10] = 0x55;
            out->data[base + 11] = 0x55;
        } break;

        case 0xAF: {
            // More key stuff (checksum)? (game expects 16 bytes in payload)
            out_size = 20;

            out->data[base + 3] = 0x00; //Command result?
        } break;

        default: {
            printf("ioboard: Unknown IC card command %02x", cmd);
        } break;
    }

	uint8_t out_size_with_parity = out_size + 1;

    out->data[0] = channel;
	out->data[1] = out_size_with_parity; // This needs to be at least 5
	out->data[base + 2] = out_size_with_parity - 5;

	uint8_t parity = 0;

	for (int i = 1; i < out_size; i++) {
		parity ^= out->data[base + i];
	}

	out->data[base + out_size] = parity;
}

void s14x_ioboard_handle_packet(void* udata, struct link_packet* in, struct link_packet* out) {
    struct s14x_ioboard* ioboard = (struct s14x_ioboard*)udata;

    // Setup header (cmd, cp, unk03, seq_number)
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
            if (ioboard->mode == 0) {
                out->data[0] = ioboard->switches & 0xff;
                out->data[1] = ioboard->switches >> 8;
            } else {
                out->data[0] = in->data[0];
            }
        } break;

        case 0x11: {
            out->data[0x32] = 0x20;
            out->data[0x36] = ioboard->buttons & 0xff;
            out->data[0x37] = ioboard->buttons >> 8;
        } break;

        case 0x31: {
            uint8_t channel = in->data[0];
            uint8_t size = in->data[1];

            if (channel == 0) {
                // Barcode Reader
                out->data[0] = channel;
                out->data[1] = 0x6;
                out->data[4 + 0] = 0x02;
                out->data[4 + 1] = 0x02;
                out->data[4 + 2] = 0x80;
                out->data[4 + 3] = 0x02;
                out->data[4 + 4] = 0x03;
                out->data[4 + 5] = 0x03;
            } else if (channel == 2) {
                ioboard_process_ic_card(in, out);
            }
        } break;

        // Used by Animal Kaiser
        case 0x41: {
            out->data[0] = 0;
        } break;

        // Used by Animal Kaiser
        case 0x51: {
            out->data[0] = 0;
        } break;

        case 0x0d: // ?
        case 0x18: // Switch
        case 0x38: // SCI
        case 0x39: // GetSwitches
        case 0x48: // CoinSensor
        case 0x58: // MechanicalSensor
        case 0xa5: { // Reset
            out->data[0] = in->data[0];
        } break;
        default: {
            printf("s14x_ioboard: Unhandled command %02x\n", in->cmd);

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

void s14x_ioboard_press_button(struct s14x_ioboard* ioboard, uint16_t mask) {
    ioboard->buttons |= mask;
}

void s14x_ioboard_release_button(struct s14x_ioboard* ioboard, uint16_t mask) {
    ioboard->buttons &= ~mask;
}