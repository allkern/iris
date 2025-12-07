#ifndef FLASH_H
#define FLASH_H

#ifdef __cplusplus
extern "C" {
#endif

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

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

#define FLASH_ID_64MBIT		0xe6
#define FLASH_ID_128MBIT	0x73
#define FLASH_ID_256MBIT	0x75
#define FLASH_ID_512MBIT	0x76
#define FLASH_ID_1024MBIT	0x79

/* SmartMedia commands.  */
#define SM_CMD_READ1		0x00
#define SM_CMD_READ2		0x01
#define SM_CMD_READ3		0x50
#define SM_CMD_RESET		0xff
#define SM_CMD_WRITEDATA	0x80
#define SM_CMD_PROGRAMPAGE	0x10
#define SM_CMD_ERASEBLOCK	0x60
#define SM_CMD_ERASECONFIRM	0xd0
#define SM_CMD_GETSTATUS	0x70
#define SM_CMD_READID		0x90

#define FLASH_CTRL_READY (1 << 0)   // r/w /BUSY
#define FLASH_CTRL_WRITE (1 << 7)   // -/w WRITE data
#define FLASH_CTRL_CSEL (1 << 8)    // -/w CS
#define FLASH_CTRL_READ (1 << 11)   // -/w READ data
#define FLASH_CTRL_NOECC (1 << 12)  // -/w ECC disabled

#define FLASH_PAGE_SIZE_BITS 9
#define FLASH_PAGE_SIZE (1 << FLASH_PAGE_SIZE_BITS)
#define FLASH_ECC_SIZE (16)
#define FLASH_PAGE_SIZE_ECC (FLASH_PAGE_SIZE + FLASH_ECC_SIZE)
#define FLASH_BLOCK_SIZE (16 * FLASH_PAGE_SIZE)
#define FLASH_BLOCK_SIZE_ECC (16 * FLASH_PAGE_SIZE_ECC)
#define FLASH_CARD_SIZE (1024 * FLASH_BLOCK_SIZE)
#define FLASH_CARD_SIZE_ECC (1024 * FLASH_BLOCK_SIZE_ECC)

struct ps2_flash {
    uint16_t cmd; // 10004804
    uint16_t addr; // 10004808
    uint16_t ctrl;  // 1000480c
    uint16_t id; // 10004814

    int counter;
    int addrbyte;
    int address;
    uint8_t data[FLASH_PAGE_SIZE_ECC];
    uint8_t file[FLASH_CARD_SIZE_ECC];
};

struct ps2_flash* ps2_flash_create(void);
int ps2_flash_init(struct ps2_flash* flash, const char* path);
void ps2_flash_destroy(struct ps2_flash* flash);
uint64_t ps2_flash_read16(struct ps2_flash* flash, uint32_t addr);
uint64_t ps2_flash_read32(struct ps2_flash* flash, uint32_t addr);
void ps2_flash_write16(struct ps2_flash* flash, uint32_t addr, uint64_t data);
void ps2_flash_write32(struct ps2_flash* flash, uint32_t addr, uint64_t data);

#ifdef __cplusplus
}
#endif

#endif