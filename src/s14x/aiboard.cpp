#include <new>

#include "aiboard.hpp"

namespace iris::s14x::aiboard {

// Reversing notes:
// The main board sends a reset command to the board, then sends an information
// request command. After that the board is kept alive and is expected to send
// status frames on its own every so often, otherwise the main board will assume
// the A.I. board is dead.
//
// By the way, I have NO idea what "A.I." stands for in this context, and not
// even the developers themselves seemed to agree, as mentioned in aiboard.hpp
// the board is also referred to as the "standalone board"
//
// We send a status fram roughly every 17ms interval. s147iolib wants at least
// 20 of them between two of its sweeps, and it sweeps every 40 passes of a loop
// that sleeps for 100ms, so this clears the bar many times over.
//
// The driver's receive ring is shared between all the nodes, so sending status 
// frames at a shorter interval and filling it with status frames starts costing
// the I/O board its packets

inline constexpr auto STATUS_INTERVAL = 5000000;

static void send_status(void* udata, int overshoot);

static void schedule_status(Aiboard* aiboard) {
    scheduler::Event event;

    event.callback = send_status;
    event.udata = aiboard;
    event.cycles = STATUS_INTERVAL;
    event.name = "AI board status";

    scheduler::schedule(aiboard->sched, event);
}

static void send_status(void* udata, int overshoot) {
    Aiboard* aiboard = (Aiboard*)udata;

    link::Packet packet = {};

    packet.src_node = NODE;
    packet.dst_node = 0;
    packet.cp = 0x38;

    packet.raw[0x38] = 0x20;
    packet.raw[0x3c] = aiboard->state & 0xff;
    packet.raw[0x3d] = aiboard->state >> 8;

    link::send_from_node(aiboard->link, NODE, packet);

    schedule_status(aiboard);
}

Aiboard* create(logger::Logger* logger, link::Link* link, scheduler::Scheduler* sched) {
    Aiboard* aiboard = new Aiboard();

    aiboard->logger = logger;
    aiboard->logger_id = logger::register_source(logger, "aiboard");

    aiboard->link = link;
    aiboard->sched = sched;

    aiboard->state = 0xffff;

    return aiboard;
}

void destroy(Aiboard* aiboard) {
    delete aiboard;
}

void press_button(Aiboard* aiboard, uint16_t mask) {
    aiboard->state &= ~mask;
}

void release_button(Aiboard* aiboard, uint16_t mask) {
    aiboard->state |= mask;
}

void handle_packet(void* udata, link::Packet* in, link::Packet* out) {
    Aiboard* aiboard = (Aiboard*)udata;

    // iris_debug(aiboard, "Standalone board command {:02x} {:02x} {:02x} {:02x} {:02x}",
    //     in->raw[0x38], in->raw[0x39], in->raw[0x3a], in->raw[0x3b], in->raw[0x3c]);

    if (!aiboard->running) {
        aiboard->running = 1;

        schedule_status(aiboard);
    }

    *out = *in;

    out->src_node = in->dst_node;
    out->dst_node = 0;
    out->cp = 0x38;

    out->raw[0x38] = (in->raw[0x38] & 0x80) ? 0x60 : 0x20;

    for (int i = 0x39; i < 0x40; i++) {
        out->raw[i] = 0;
    }
}

}
