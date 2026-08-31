#pragma once

#include "u128.h"
#include "logger.hpp"

#include "dmac.hpp"
#include "gif.hpp"
#include "intc.hpp"
#include "timers.hpp"
#include "vif.hpp"
#include "ipu/ipu.hpp"
#include "../iop/usb.hpp"

#include "shared/ram.hpp"
#include "shared/sif.hpp"
#include "shared/bios.hpp"
#include "shared/sbus.hpp"
#include "shared/dev9.hpp"
#include "shared/speed.hpp"

namespace iris::gs { struct Gs; }
namespace iris::cdvd { struct Cdvd; }

namespace iris::ee::bus {

struct Bus {
    // EE-only
    ram::Ram* ee_ram;
    dmac::Dmac* dmac;
    gif::Gif* gif;
    gs::Gs* gs;
    ipu::Ipu* ipu;
    intc::Intc* intc;
    vif::Vif* vif0;
    vif::Vif* vif1;
    vu::Vu* vu0;
    vu::Vu* vu1;
    timers::Timers* timers;
    
    // EE/IOP
    cdvd::Cdvd* cdvd;
    usb::Usb* usb;
    bios::Bios* bios;
    bios::Bios* rom1;
    bios::Bios* rom2;
    ram::Ram* iop_ram;
    sif::Sif* sif;
    sbus::Sbus* sbus;
    dev9::Dev9* dev9;
    speed::Speed* speed;

    // 0x2000 bytes per entry, so these cover the first 512 MB
    void* fastmem_r_table[0x10000];
    void* fastmem_w_table[0x10000];

    // So we can invalidate IOP blocks on EE -> IOP RAM writes
    iop::Iop* iop;

    uint32_t mch_ricm;
    uint32_t mch_drd;
    uint32_t rdram_sdevid;

    void (*kputchar)(void*, char);
    void* kputchar_udata;

    logger::Logger* logger = nullptr;
    size_t logger_id = 0;
};

void init_kputchar(Bus* bus, void (*kputchar)(void*, char), void* udata);
void init_fastmem(Bus* bus, int ee_ram_size, int iop_ram_size);

}
