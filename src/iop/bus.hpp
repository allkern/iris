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

#include "u128.h"

// Shared IOP/EE hardware
#include "shared/ram.hpp"
#include "shared/sif.hpp"
#include "shared/bios.hpp"
#include "shared/sbus.hpp"
#include "shared/dev9.hpp"
#include "shared/speed.hpp"

// IOP-only hardware
#include "dma.hpp"
#include "intc.hpp"
#include "timers.hpp"
#include "cdvd.hpp"
#include "rainbow.hpp"
#include "sio2.hpp"
#include "spu2.hpp"
#include "usb.hpp"
#include "fw.hpp"

// Arcade hardware
#include "s14x/nand.hpp"
#include "s14x/syscon.hpp"
#include "s14x/sram.hpp"
#include "s14x/link.hpp"
#include "s2x6/acata.hpp"
#include "s2x6/acjv.hpp"
#include "s2x6/acsram.hpp"
#include "s2x6/acram.hpp"
#include "s2x6/accore.hpp"
#include "s2x6/acuart.hpp"
#include "logger.hpp"

namespace iris::iop::bus {

inline constexpr auto FASTMEM_BLKSIZE = 0x2000;
inline constexpr auto FASTMEM_TBLSIZE = (0x20000000 / FASTMEM_BLKSIZE);

struct Bus {
    bios::Bios* bios;
    bios::Bios* rom1;
    bios::Bios* rom2;
    ram::Ram* iop_ram;
    ram::Ram* iop_spr;
    sif::Sif* sif;
    dma::Dma* dma;
    iop::intc::Intc* intc;
    iop::timers::Timers* timers;
    cdvd::Cdvd* cdvd;
    rainbow::Rainbow* rainbow;
    sio2::Sio2* sio2;
    spu2::Spu2* spu2;
    usb::Usb* usb;
    fw::Fw* fw;
    sbus::Sbus* sbus;
    dev9::Dev9* dev9;
    speed::Speed* speed;

    // Arcade hardware
    s14x::nand::Nand* s14x_nand;
    s14x::syscon::Syscon* s14x_syscon;
    s14x::sram::Sram* s14x_sram;
    s14x::link::Link* s14x_link;
    s2x6::acata::Acata* s2x6_acata;
    s2x6::acjv::Acjv* s2x6_acjv;
    s2x6::acsram::Acsram* s2x6_acsram;
    s2x6::acram::Acram* s2x6_acram;
    s2x6::accore::Accore* s2x6_accore;
    s2x6::acuart::Acuart* s2x6_acuart;

    int disable_usb;

    void* fastmem_r_table[0x10000];
    void* fastmem_w_table[0x10000];

    void (*invalidate_cache)(void* udata, uint32_t addr);
    void* invalidate_cache_udata;

    logger::Logger* logger = nullptr;
    size_t logger_id = 0;
};

void set_usb_disabled(Bus* bus, int disabled);
int is_usb_disabled(Bus* bus);

void init_fastmem(Bus* bus, int ram_size);

}
