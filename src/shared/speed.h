#ifndef SPEED_H
#define SPEED_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include "iop/intc.h"
#include "speed/ata.h"
#include "speed/flash.h"

// SPEED is a chip presented as a register interface to speed devices.
// This includes:
// - SMAP (Ethernet?) at 0x10000100
// - ATA (HDD) at 0x10000040
// - UART (Maxim MAX232 UART, used in arcade units?) at ?
// - DVR (PSX DESR Digital Video Recorder)
// - Flash (also known as EROM, used in PSX DESR units) at 0x10004800
// It is mapped to phys 0x10000000 on the IOP side, and phys 0x14000000
// on the EE side.
// Software won't even try to access SPEED if it isn't detected through
// the speed_REV (1f80146e) register first.

// Note: SMAP also provides access to the unit's MAC address (used by the BIOS)

// Notes: USB and i.Link (FireWire) are completely separate
//        and not connected to the SPEED interface at all.
// 
//        USB is presented through an OHCI interface at 0x1F801600
//        i.Link is presented through a FireWire interface at 0x1F808400

#define SPD_INTR_ATA0   0x0001
#define SPD_INTR_ATA1   0x0002
#define SPD_INTR_DVR    0x0200 // mask=0x0200
#define SPD_INTR_UART   0x1000 // mask=0x1000
#define SPD_INTR_ATA    (SPD_INTR_ATA0 | SPD_INTR_ATA1) // mask=0x0003

#define SPD_CAPS_SMAP  (1 << 0)
#define SPD_CAPS_ATA   (1 << 1)
#define SPD_CAPS_UART  (1 << 3)
#define SPD_CAPS_DVR   (1 << 4)
#define SPD_CAPS_FLASH (1 << 5)

struct ps2_speed {
    uint16_t rev; // 10000000
    uint16_t rev1; // 10000002
    uint16_t rev3; // 10000004
    uint16_t rev8; // 1000000e
    uint32_t dma_ctrl; // 10000024
    uint16_t intr_stat; // 10000028
    uint16_t intr_mask; // 1000002a
    uint16_t pio_dir; // 1000002c
    uint16_t pio_data; // 1000002e
    uint32_t xfr_ctrl; // 10000032
    uint32_t unknown38; // 10000038
    uint32_t if_ctrl; // 10000064
    uint32_t pio_mode; // 10000070
    uint32_t mwdma_mode; // 10000072
    uint32_t udma_mode; // 10000074

    // FLASH (XFROM) registers
    struct ps2_ata* ata;
    struct ps2_flash* flash;

    struct ps2_iop_intc* iop_intc;
};

struct ps2_speed* ps2_speed_create(void);
void ps2_speed_init(struct ps2_speed* speed, struct ps2_iop_intc* iop_intc);
void ps2_speed_destroy(struct ps2_speed* speed);
uint64_t ps2_speed_read8(struct ps2_speed* speed, uint32_t addr);
uint64_t ps2_speed_read16(struct ps2_speed* speed, uint32_t addr);
uint64_t ps2_speed_read32(struct ps2_speed* speed, uint32_t addr);
void ps2_speed_write8(struct ps2_speed* speed, uint32_t addr, uint64_t data);
void ps2_speed_write16(struct ps2_speed* speed, uint32_t addr, uint64_t data);
void ps2_speed_write32(struct ps2_speed* speed, uint32_t addr, uint64_t data);
void ps2_speed_send_irq(struct ps2_speed* speed, uint16_t irq);

#ifdef __cplusplus
}
#endif

#endif