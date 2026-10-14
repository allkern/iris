/*
    This file is part of the Iris PlayStation 2 emulator project.
    Hosted at https://github.com/allkern/iris

    MIT License

    Copyright (c) 2026 Allkern/Lycoder (Lisandro Alarcón)

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in all
    copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
    SOFTWARE.
*/

#pragma once

#include <unordered_map>

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
namespace iris::mtvu { struct Mtvu; }

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
    mtvu::Mtvu* mtvu;
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

    std::unordered_map <uint32_t, uint32_t> patches;

    logger::Logger* logger = nullptr;
    size_t logger_id = 0;
};

void init_kputchar(Bus* bus, void (*kputchar)(void*, char), void* udata);
void init_fastmem(Bus* bus, int ee_ram_size, int iop_ram_size);
void set_patch(Bus* bus, uint32_t addr, uint32_t value);
void clear_patches(Bus* bus);

}
