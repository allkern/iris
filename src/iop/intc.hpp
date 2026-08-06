#pragma once

#include "iop.hpp"
#include "logger.hpp"

namespace iris::iop::intc {

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
  13    IRQ13  DEV9
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

enum Source : uint32_t {
    VBLANK_IN = 0x00000001,

    // Bit 1 in PS2 mode is mapped to the SBUS IRQ, in PS1 mode
    // it's mapped to the PS1 GPU
    SBUS = 0x00000002,
    GPU = 0x00000002,
    CDVD = 0x00000004,
    DMA = 0x00000008,
    TIMER0 = 0x00000010,
    TIMER1 = 0x00000020,
    TIMER2 = 0x00000040,
    SIO0 = 0x00000080,
    SIO1 = 0x00000100,
    SPU2 = 0x00000200,
    PIO = 0x00000400,
    VBLANK_OUT = 0x00000800,
    DVD = 0x00001000,
    DEV9 = 0x00002000,
    TIMER3 = 0x00004000,
    TIMER4 = 0x00008000,
    TIMER5 = 0x00010000,
    SIO2 = 0x00020000,
    HTR0 = 0x00040000,
    HTR1 = 0x00080000,
    HTR2 = 0x00100000,
    HTR3 = 0x00200000,
    USB = 0x00400000,
    EXTR = 0x00800000,
    FWRE = 0x01000000,
    FDMA = 0x02000000
};

struct Intc {
    struct {
        iop::Iop* iop;
    } hw;

    uint32_t stat;
    uint32_t mask;
    uint32_t ctrl;


    logger::Logger* logger = nullptr;
    size_t logger_id = 0;
};

Intc* create(logger::Logger* logger, iop::Iop* iop);
void reset(Intc* intc);
void irq(Intc* intc, int dev);
void destroy(Intc* intc);
uint64_t read8(Intc* intc, uint32_t addr);
uint64_t read16(Intc* intc, uint32_t addr);
uint64_t read32(Intc* intc, uint32_t addr);
void write8(Intc* intc, uint32_t addr, uint64_t data);
void write16(Intc* intc, uint32_t addr, uint64_t data);
void write32(Intc* intc, uint32_t addr, uint64_t data);

}
