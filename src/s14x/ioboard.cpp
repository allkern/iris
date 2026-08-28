#include <new>

#include "ioboard.hpp"

namespace iris::s14x::ioboard {

Ioboard* create(logger::Logger* logger, int mode) {
    Ioboard* ioboard = new Ioboard();

    ioboard->logger = logger;
    ioboard->logger_id = logger::register_source(logger, "ioboard");

    ioboard->version = 0x0104;
    ioboard->switches = 0xffff;
    ioboard->mode = mode;

    return ioboard;
}

void destroy(Ioboard* ioboard) {
    delete ioboard;
}

// Animal Kaiser vets the IC card reader by XORing the eight bytes command ACh
// returns, which have to come out zero, and by adding up the sixteen command
// AFh returns, which have to come out 889h
inline constexpr auto CERTIFICATE_SIZE = 16;
inline constexpr auto CERTIFICATE_FILL = 0x88;
inline constexpr auto CERTIFICATE_SUM = 0x889;

uint8_t link_calculate_checksum(link::Packet* packet) {
    uint8_t checksum = 0;

    for (int i = 0; i < sizeof(packet->data); i++) {
        checksum += packet->data[i];
    }

    return checksum;
}

void process_ic_card(Ioboard* ioboard, link::Packet* in, link::Packet* out) {
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
            // Animal Kaiser adds these 16 bytes up and will not accept
            // the reader unless they total 889h, and nothing else ever
            // reads them, so any 16 bytes that add up will do
            out_size = 20;

            out->data[base + 3] = 0x00; //Command result?

            uint16_t sum = 0;

            for (int i = 0; i < CERTIFICATE_SIZE - 1; i++) {
                out->data[base + 4 + i] = CERTIFICATE_FILL;

                sum += CERTIFICATE_FILL;
            }

            out->data[base + 4 + CERTIFICATE_SIZE - 1] = CERTIFICATE_SUM - sum;
        } break;

        default: {
            iris_debug(ioboard, "Unknown IC card command {:02x}", cmd);
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

void handle_packet(void* udata, link::Packet* in, link::Packet* out) {
    Ioboard* ioboard = (Ioboard*)udata;

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
                process_ic_card(ioboard, in, out);
            }
        } break;

        case 0x41: {
            int record = 0;

            for (int i = 0; i < NUM_COIN_SLOTS; i++) {
                if (!ioboard->coins[i])
                    continue;

                out->data[record + 0] = 0x30 + i + 1;
                out->data[record + 1] = 0;
                out->data[record + 2] = ioboard->coins[i] & 0xff;
                out->data[record + 3] = ioboard->coins[i] >> 8;

                ioboard->coins[i] = 0;

                record += 4;
            }

            out->data[record] = 0;
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
            iris_debug(ioboard, "Ioboard: Unhandled command {:02x}", in->cmd);

            out->data[0] = in->data[0];
        } break;
    }

    out->checksum = link_calculate_checksum(out);
}

void press_switch(Ioboard* ioboard, uint16_t mask) {
    ioboard->switches &= ~mask;
}

void release_switch(Ioboard* ioboard, uint16_t mask) {
    ioboard->switches |= mask;
}

void press_button(Ioboard* ioboard, uint16_t mask) {
    ioboard->buttons |= mask;
}

void release_button(Ioboard* ioboard, uint16_t mask) {
    ioboard->buttons &= ~mask;
}

void set_coin_switch(Ioboard* ioboard, int slot, int pressed) {
    if (slot < 0 || slot >= NUM_COIN_SLOTS)
        return;

    uint16_t mask = 1 << slot;

    if (pressed && !(ioboard->coin_switches & mask)) {
        ioboard->coins[slot]++;
    }

    if (pressed) {
        ioboard->coin_switches |= mask;
    } else {
        ioboard->coin_switches &= ~mask;
    }
}

}
