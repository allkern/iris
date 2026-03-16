/*  s14x/aiboard.h - Namco System 147/148 I/O board emulation

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

#ifndef S14X_AIBOARD_H
#define S14X_AIBOARD_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdio.h>

#include "link.h"

struct s14x_aiboard {
    uint16_t version;
};

struct s14x_aiboard* s14x_aiboard_create(void);
int s14x_aiboard_init(struct s14x_aiboard* aiboard);
void s14x_aiboard_destroy(struct s14x_aiboard* aiboard);

void s14x_aiboard_handle_packet(void* udata, struct link_packet* in, struct link_packet* out);

#ifdef __cplusplus
}
#endif

#endif