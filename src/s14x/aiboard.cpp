#include <new>

#include "aiboard.hpp"

namespace iris::s14x::aiboard {

Aiboard* create(logger::Logger* logger) {
    Aiboard* aiboard = new Aiboard();

    aiboard->logger = logger;
    aiboard->logger_id = logger::register_source(logger, "aiboard");

    aiboard->version = 0x0104;

    return aiboard;
}

void destroy(Aiboard* aiboard) {
    delete aiboard;
}

void handle_packet(void* udata, link::Packet* in, link::Packet* out) {
    Aiboard* aiboard = (Aiboard*)udata;

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

    //     iop::intc::irq(link->intc, iop::intc::DEV9);

    //     return;
    // }
}

}
