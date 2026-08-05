#pragma once
#include "logger.hpp"

// speed.hpp includes this header, so the parent stays a forward
// declaration here and is included for real in the .cpp
namespace iris::speed { struct Speed; }

namespace iris::speed::flash {

/*
    SPEED FLASH docs
    ----------------

    Also known as XFROM (eXternal Flash ROM), this flash memory is connected to
    the SPEED chip, and is accessed through the registers listed below.
    The actual chip seems to be the same chip used for the memory cards, but bigger.
    The chip is controlled using "SmartMedia" commands sent through the COMMAND register.

    Registers (X=0 IOP side, X=4 EE side):
    1X004800 - DATA register
        Used by WRITEDATA/PROGRAMPAGE/READ3/READ1, this is where data is
        written or read from
    1X004804 - CMD register
        Write to this register to send a command
    1X004808 - ADDR register
        Probably used to set page offsets
    1X00480c - CTRL register
        bit 0 seems to be some kind of READY bit
    1X004810 - unused?
    1X004814 - ID/STATUS? (used by READID/GETSTATUS)
        Contains the size (or ID) of the flash chip after sending READID
        and some kind of status after sending GETSTATUS.

    SmartMedia commands list:
    0x00 - READ1
    0x01 - READ2
    0x50 - READ3
    0xff - RESET
    0x80 - WRITEDATA
    0x10 - PROGRAMPAGE
    0x60 - ERASEBLOCK
    0xd0 - ERASECONFIRM
    0x70 - GETSTATUS
    0x90 - READID
*/


inline constexpr auto ID_64MBIT = 0xe6;
inline constexpr auto ID_128MBIT = 0x73;
inline constexpr auto ID_256MBIT = 0x75;
inline constexpr auto ID_512MBIT = 0x76;
inline constexpr auto ID_1024MBIT = 0x79;

/* SmartMedia commands.  */
inline constexpr auto SM_CMD_READ1 = 0x00;
inline constexpr auto SM_CMD_READ2 = 0x01;
inline constexpr auto SM_CMD_READ3 = 0x50;
inline constexpr auto SM_CMD_RESET = 0xff;
inline constexpr auto SM_CMD_WRITEDATA = 0x80;
inline constexpr auto SM_CMD_PROGRAMPAGE = 0x10;
inline constexpr auto SM_CMD_ERASEBLOCK = 0x60;
inline constexpr auto SM_CMD_ERASECONFIRM = 0xd0;
inline constexpr auto SM_CMD_GETSTATUS = 0x70;
inline constexpr auto SM_CMD_READID = 0x90;

inline constexpr auto CTRL_READY = (1 << 0);// r/w /BUSY
inline constexpr auto CTRL_WRITE = (1 << 7);// -/w WRITE data
inline constexpr auto CTRL_CSEL = (1 << 8);// -/w CS
inline constexpr auto CTRL_READ = (1 << 11);// -/w READ data
inline constexpr auto CTRL_NOECC = (1 << 12);// -/w ECC disabled

inline constexpr auto PAGE_SIZE_BITS = 9;
inline constexpr auto PAGE_SIZE = (1 << PAGE_SIZE_BITS);
inline constexpr auto ECC_SIZE = (16);
inline constexpr auto PAGE_SIZE_ECC = (PAGE_SIZE + ECC_SIZE);
inline constexpr auto BLOCK_SIZE = (16 * PAGE_SIZE);
inline constexpr auto BLOCK_SIZE_ECC = (16 * PAGE_SIZE_ECC);
inline constexpr auto CARD_SIZE = (1024 * BLOCK_SIZE);
inline constexpr auto CARD_SIZE_ECC = (1024 * BLOCK_SIZE_ECC);

struct Flash {
    uint16_t cmd; // 10004804
    uint16_t addr; // 10004808
    uint16_t ctrl;  // 1000480c
    uint16_t id; // 10004814

    int counter;
    int addrbyte;
    int address;
    uint8_t data[PAGE_SIZE_ECC];
    uint8_t file[CARD_SIZE_ECC];

    logger::Logger* logger = nullptr;
    size_t logger_id = 0;
};

Flash* create(logger::Logger* logger);
void init(Flash* flash);
int load(Flash* flash, const char* path);
void destroy(Flash* flash);
uint64_t read16(Flash* flash, uint32_t addr);
uint64_t read32(Flash* flash, uint32_t addr);
void write16(Flash* flash, uint32_t addr, uint64_t data);
void write32(Flash* flash, uint32_t addr, uint64_t data);

}
