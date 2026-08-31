/*  kp2/icca.h - Konami ACIO IC card reader (ICCA)

    Commands
    --------
    Code     Description
    0002h    get node info
    0000h    clear
    0003h    reset
    0116h    unknown
    0120h    unknown
    0130h    start the queue loop
    0131h    tells the reader to go and read the card
    0134h    poll
    0135h    set the slot state from payload byte 1
    013ah    device control, sleep
    0160h    key exchange
    0161h    poll felica
    0164h    encrypted poll

    Status block (10h bytes)
    ------------------------
    Offset   Description
    00h      status, 01h idle, 02h card in the reader, 04h idle on a
            wavepass reader, 00h fault
    01h      sensors
    02h-09h  card uid
    0ah      card type
    0bh      03h once the keypad is up, games wait on this
    0ch-0dh  key event, the low nibble is the key and the high nibble a
            counter that steps every press
    0eh-0fh  keys currently held, high byte first

    node info (2Ch bytes)
    ---------------------
    Offset   Description
    00h-03h  node type
    04h      flags
    05h-07h  version.
    08h-0bh  product code, "icca"
    0ch-1bh  build date
    1ch-2bh  build time
*/

#pragma once

#include <cstdint>

#include "acio.hpp"

namespace iris::kp2::icca {

inline constexpr auto UID_SIZE = 8;
inline constexpr auto STATUS_SIZE = 16;
inline constexpr auto INFO_SIZE = 44;

inline constexpr auto SLOT_CLOSE = 0x00;
inline constexpr auto SLOT_OPEN = 0x11;
inline constexpr auto SLOT_EJECT = 0x12;

enum Key : uint16_t {
    KEY_EMPTY = 0x0001,
    KEY_3 = 0x0002,
    KEY_6 = 0x0004,
    KEY_9 = 0x0008,
    KEY_0 = 0x0100,
    KEY_1 = 0x0200,
    KEY_4 = 0x0400,
    KEY_7 = 0x0800,
    KEY_00 = 0x1000,
    KEY_2 = 0x2000,
    KEY_5 = 0x4000,
    KEY_8 = 0x8000
};

struct Icca {
    uint8_t uid[UID_SIZE];

    int card_present;
    int card_switch;
    int accepting;
    int holding;

    uint16_t keypad;
    uint16_t keypad_last;

    uint8_t key_event;
    uint8_t key_counter;
};

void init(Icca* icca);
void set_uid(Icca* icca, const uint8_t* uid);
void set_card_switch(Icca* icca, int pressed);
void press_key(Icca* icca, uint16_t mask);
void release_key(Icca* icca, uint16_t mask);
bool handle_packet(void* udata, const acio::Request* request, acio::Response* response);

}
