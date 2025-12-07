#ifndef ATA_H
#define ATA_H

#ifdef __cplusplus
extern "C" {
#endif

/*
    SPEED ATA docs
    --------------

    This is basically just a standard ATA interface connected to the SPEED chip.
    It's mapped starting at 10000040 (EE side 14000040).

    Registers (X=0 IOP side, X=4 EE side):
    1X000040 - DATA
    1X000042 - ERROR
             - FEATURE
    1X000044 - NSECTOR
    1X000046 - SECTOR
    1X000048 - LCYL
    1X00004a - HCYL
    1X00004c - SELECT
    1X00004e - STATUS
             - COMMAND
    1X000050 - 1X00004B - unused
    1X00005c - CONTROL
*/

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

#include "../speed.h"

struct ps2_ata {
    uint16_t data;
    uint16_t error;
    uint16_t feature;
    uint16_t nsector;
    uint16_t sector;
    uint16_t lcyl;
    uint16_t hcyl;
    uint16_t select;
    uint16_t status;
    uint16_t command;
    uint16_t control;

    struct ps2_speed* speed;
};

struct ps2_ata* ps2_ata_create(void);
int ps2_ata_init(struct ps2_ata* ata, struct ps2_speed* speed, const char* path);
void ps2_ata_destroy(struct ps2_ata* ata);
uint64_t ps2_ata_read16(struct ps2_ata* ata, uint32_t addr);
uint64_t ps2_ata_read32(struct ps2_ata* ata, uint32_t addr);
void ps2_ata_write16(struct ps2_ata* ata, uint32_t addr, uint64_t data);
void ps2_ata_write32(struct ps2_ata* ata, uint32_t addr, uint64_t data);

#ifdef __cplusplus
}
#endif

#endif