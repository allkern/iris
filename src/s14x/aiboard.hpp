/*  s14x/aiboard.h - Namco System 147/148 standalone ("A.I.") board emulation

    Notes: pacmanbr has a third board on the CircLink network at node 3, which
           s147iolib calls the standalone P.C.B. and the test menu calls the
           A.I. board. The main board addresses it with the driver's "special
           packets", whose continuation pointer of 38h leaves eight payload
           bytes at the tail of the frame, and the board answers and talks
           back in the same form.

           Payload format (offsets into the 64 byte frame)
           -----------------------------------------------
           Offset   Description
           38h      Command
                    80h  Request from the main board, the command itself is
                         repeated at 3Ah and 3Ch (8Fh reset, 01h information,
                         07h and 03h peripheral configuration)
                    00h  State push from the main board
                    20h  Status frame from the board, carrying a 16 bit word
                         at 3Ch-3Dh
                    60h  Answer to a request. s147iolib takes this as the end
                         of a transaction and zeroes the board's frame counter,
                         so it can only be sent when one was asked for
           39h      Bit 0 set means the board is still booting
           3Ah-3Dh  Board information, printed by s147iolib but not checked
           3Fh      Unused, the special packet path carries no checksum

           The board runs standalone, once reset it puts a status frame
           on the network on its own, and s147iolib counts those. Fewer than
           20 between two of its sweeps and the board is reported missing,
           in which case the test menu reports "3-13 I/O ERROR 4".
*/

#pragma once

#include "link.hpp"
#include "scheduler.hpp"
#include "logger.hpp"

namespace iris::s14x::aiboard {

inline constexpr auto NODE = 3;

struct Aiboard {
    uint16_t state;

    int running;

    link::Link* link = nullptr;
    scheduler::Scheduler* sched = nullptr;

    logger::Logger* logger = nullptr;
    size_t logger_id = 0;
};

Aiboard* create(logger::Logger* logger, link::Link* link, scheduler::Scheduler* sched);
void destroy(Aiboard* aiboard);

void handle_packet(void* udata, link::Packet* in, link::Packet* out);

}
