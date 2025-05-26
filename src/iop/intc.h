#ifndef IOP_INTC_H
#define IOP_INTC_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include "iop.h"

/*
  0     IRQ0   VBLANK start
  1     IRQ1   GPU (used in PSX mode)
  2     IRQ2   CDVD Drive
  3     IRQ3   DMA
  4     IRQ4   Timer 0
  5     IRQ5   Timer 1
  6     IRQ6   Timer 2
  7     IRQ7   SIO0
  8     IRQ8   SIO1
  9     IRQ9   SPU2
  10    IRQ10  PIO
  11    IRQ11  VBLANK end
  12    IRQ12  DVD? (unknown purpose)
  13    IRQ13  PCMCIA (related to DEV9 expansion slot)
  14    IRQ14  Timer 3
  15    IRQ15  Timer 4
  16    IRQ16  Timer 5
  17    IRQ17  SIO2
  18    IRQ18  HTR0? (unknown purpose)
  19    IRQ19  HTR1?
  20    IRQ20  HTR2?
  21    IRQ21  HTR3?
  22    IRQ22  USB
  23    IRQ23  EXTR? (unknown purpose)
  24    IRQ24  FWRE (related to FireWire)
  25    IRQ25  FDMA? (FireWire DMA?)
*/

#define IOP_INTC_VBLANK_IN  0x00000001
#define IOP_INTC_GPU        0x00000002
#define IOP_INTC_CDVD       0x00000004
#define IOP_INTC_DMA        0x00000008
#define IOP_INTC_TIMER0     0x00000010
#define IOP_INTC_TIMER1     0x00000020
#define IOP_INTC_TIMER2     0x00000040
#define IOP_INTC_SIO0       0x00000080
#define IOP_INTC_SIO1       0x00000100
#define IOP_INTC_SPU2       0x00000200
#define IOP_INTC_PIO        0x00000400
#define IOP_INTC_VBLANK_OUT 0x00000800
#define IOP_INTC_DVD        0x00001000
#define IOP_INTC_PCMCIA     0x00002000
#define IOP_INTC_TIMER3     0x00004000
#define IOP_INTC_TIMER4     0x00008000
#define IOP_INTC_TIMER5     0x00010000
#define IOP_INTC_SIO2       0x00020000
#define IOP_INTC_HTR0       0x00040000
#define IOP_INTC_HTR1       0x00080000
#define IOP_INTC_HTR2       0x00100000
#define IOP_INTC_HTR3       0x00200000
#define IOP_INTC_USB        0x00400000
#define IOP_INTC_EXTR       0x00800000
#define IOP_INTC_FWRE       0x01000000
#define IOP_INTC_FDMA       0x02000000

struct ps2_iop_intc {
    uint32_t stat;
    uint32_t mask;
    uint32_t ctrl;

    struct iop_state* iop;
};

struct ps2_iop_intc* ps2_iop_intc_create(void);
void ps2_iop_intc_init(struct ps2_iop_intc* intc, struct iop_state* iop);
void ps2_iop_intc_irq(struct ps2_iop_intc* intc, int dev);
void ps2_iop_intc_destroy(struct ps2_iop_intc* intc);
uint64_t ps2_iop_intc_read8(struct ps2_iop_intc* intc, uint32_t addr);
uint64_t ps2_iop_intc_read16(struct ps2_iop_intc* intc, uint32_t addr);
uint64_t ps2_iop_intc_read32(struct ps2_iop_intc* intc, uint32_t addr);
void ps2_iop_intc_write8(struct ps2_iop_intc* intc, uint32_t addr, uint64_t data);
void ps2_iop_intc_write16(struct ps2_iop_intc* intc, uint32_t addr, uint64_t data);
void ps2_iop_intc_write32(struct ps2_iop_intc* intc, uint32_t addr, uint64_t data);

#ifdef __cplusplus
}
#endif

#endif