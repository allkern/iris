/*  s14x/ioboard.h - Namco System 147/148 I/O board emulation

    Notes: The I/O board is connected to the main board via the CircLink
           network at node 2. The main board sends ARCNET packets with
           commands through CircLink and the I/O board responds with data.

           Packet format
           -------------
           Offset   Description
           00h      Source node (e.g. 1 for the main board)
           01h      Destination node
           02h      Unknown (driver will only accept 04h)
           03h      Unknown (driver will only accept 00h)
           04h      Sequence number
                    Note: This byte needs to contain the same sequence number
                    that was sent by the main board.
           05h      Command
           06h-3Eh  Data
           3Fh      Checksum (sum of 06h-3Eh)

           I/O board commands
           ------------------
           Command  Description
           0Dh      Unknown (?)
           0Fh      Get PCB information (returns version 0000:0104h)
           10h      Depends on the board, on pacmanap this returns the
                    state of the switches
           18h      Switch (?)
           38h      SCI (?)
           39h      Get switches (?)
           48h      Coin sensor (?)
           58h      Mechanical sensor (?)
           A5h      Reset

           There are actually more commands but we stub most of them by
           returning the same data that was sent by the main board.
*/

#pragma once

#include "link.hpp"
#include "logger.hpp"

namespace iris::s14x::ioboard {

enum Switch : uint16_t {
    DOWN = 0x0001,
    UP = 0x0002,
    ENTER = 0x0004,
    TEST = 0x0008,
    SERVICE = 0x0020,
    P4_START = 0x0100,
    P3_START = 0x0200,
    P2_START = 0x0400,
    P1_START = 0x0800
};

enum Button : uint16_t {
    P4_UP = 0x0001,
    P4_DOWN = 0x0002,
    P4_RIGHT = 0x0004,
    P4_LEFT = 0x0008,
    P2_UP = 0x0010,
    P2_DOWN = 0x0020,
    P2_RIGHT = 0x0040,
    P2_LEFT = 0x0080,
    P3_UP = 0x0100,
    P3_DOWN = 0x0200,
    P3_RIGHT = 0x0400,
    P3_LEFT = 0x0800,
    P1_UP = 0x1000,
    P1_DOWN = 0x2000,
    P1_RIGHT = 0x4000,
    P1_LEFT = 0x8000
};

struct Ioboard {
    uint16_t version;
    uint16_t switches;
    uint16_t buttons;

    int mode;

    logger::Logger* logger = nullptr;
    size_t logger_id = 0;
};

Ioboard* create(logger::Logger* logger, int mode);
void destroy(Ioboard* ioboard);

void press_switch(Ioboard* ioboard, uint16_t mask);
void release_switch(Ioboard* ioboard, uint16_t mask);
void press_button(Ioboard* ioboard, uint16_t mask);
void release_button(Ioboard* ioboard, uint16_t mask);

void handle_packet(void* udata, link::Packet* in, link::Packet* out);

}
