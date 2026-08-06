#pragma once

#include "ps2_decl.hpp"

#include "ee/bus.hpp"
#include "ee/ee.hpp"
#include "ee/gif.hpp"
#include "ee/vif.hpp"
#include "ee/dmac.hpp"
#include "ee/intc.hpp"
#include "ee/timers.hpp"
#include "ee/vu.hpp"
#include "iop/bus.hpp"
#include "iop/bus_decl.hpp"
#include "iop/iop.hpp"
#include "iop/dma.hpp"
#include "iop/intc.hpp"
#include "iop/timers.hpp"
#include "iop/cdvd.hpp"
#include "iop/sio2.hpp"
#include "iop/spu2.hpp"
#include "iop/usb.hpp"
#include "iop/fw.hpp"
#include "shared/bios.hpp"
#include "shared/ram.hpp"
#include "shared/sif.hpp"
#include "shared/sbus.hpp"
#include "shared/dev9.hpp"
#include "shared/speed.hpp"
#include "gs/gs.hpp"
#include "ipu/ipu.hpp"

// Arcade hardware
// Namco System 147/148
#include "s14x/nand.hpp"
#include "s14x/syscon.hpp"
#include "s14x/sram.hpp"
#include "s14x/link.hpp"
#include "s14x/ioboard.hpp"
#include "s14x/aiboard.hpp"

// Namco System 246/256
#include "s2x6/acata.hpp"
#include "s2x6/acjv.hpp"

// SIO2 devices (controllers, memory cards, etc.)
#include "dev/ds.hpp"
#include "dev/guncon.hpp"
#include "dev/mcd.hpp"
#include "dev/ps1_mcd.hpp"
#include "dev/mtap.hpp"

#include "scheduler.hpp"
#include "logger.hpp"
#include "rom.hpp"

namespace iris::ee::dmac { struct Dmac; }
namespace iris::ee::bus { struct Bus; }
namespace iris::ee { struct Ee; }

namespace iris::ps2 {

struct ElfFunction {
    char* name;
    uint32_t addr;
};

struct Ps2 {
    // CPUs
    ee::Ee* ee;
    iop::Iop* iop;
    vu::Vu* vu0;
    vu::Vu* vu1;

    // EE-only
    ee::bus::Bus* ee_bus;
    gif::Gif* gif;
    vif::Vif* vif0;
    vif::Vif* vif1;
    gs::Gs* gs;
    ipu::Ipu* ipu;
    ee::dmac::Dmac* ee_dma;
    ram::Ram* ee_ram;
    ee::intc::Intc* ee_intc;
    ee::timers::Timers* ee_timers;

    // IOP-only
    iop::bus::Bus* iop_bus;
    ram::Ram* iop_spr;
    iop::dma::Dma* iop_dma;
    iop::intc::Intc* iop_intc;
    iop::timers::Timers* iop_timers;
    sio2::Sio2* sio2;
    spu2::Spu2* spu2;
    fw::Fw* fw;
        
    // Shared
    ram::Ram* iop_ram;
    bios::Bios* bios;
    bios::Bios* rom1; // Mapped to 1E000000-1E3FFFFF (DVD firmware)
    bios::Bios* rom2; // Mapped to 1E400000-1E7FFFFF (Chinese exts)
    cdvd::Cdvd* cdvd;
    sif::Sif* sif;
    usb::Usb* usb;
    sbus::Sbus* sbus;
    dev9::Dev9* dev9;
    speed::Speed* speed;

    // Namco System 147/148
    s14x::nand::Nand* s14x_nand;
    s14x::syscon::Syscon* s14x_syscon;
    s14x::sram::Sram* s14x_sram;
    s14x::link::Link* s14x_link;
    s14x::ioboard::Ioboard* s14x_ioboard;
    s14x::aiboard::Aiboard* s14x_aiboard;

    // Namco System 246/256/Super 256
    s2x6::acata::Acata* s2x6_acata;
    s2x6::acjv::Acjv* s2x6_acjv;

    scheduler::Scheduler* sched;

    int ee_cycles;
    int iop_cycles;
    int timescale;
    int system, detected_system;

    rom::Info rom0_info;
    rom::Info rom1_info;

    logger::Logger* logger;
    size_t logger_id;
};

}