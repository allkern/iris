#pragma once

#include "u128.h"

#include "shared/sif.hpp"

#include "intc.hpp"
#include "scheduler.hpp"
#include "s2x6/acata.hpp"

#include "bus_decl.hpp"

#include "ee/dmac.hpp"
#include "logger.hpp"

// These include dma.h in turn, so they stay forward declarations here
namespace iris::cdvd { struct Cdvd; }
namespace iris::sio2 { struct Sio2; }
namespace iris::spu2 { struct Spu2; }

namespace iris::ee::dmac { struct Dmac; }

namespace iris::iop::dma {

enum ChannelId : int {
    MDEC_IN,
    MDEC_OUT,
    SIF2,
    CDVD,
    SPU1,
    PIO,
    OTC,
    SPU2,
    DEV9,
    SIF0,
    SIF1,
    SIO2_IN,
    SIO2_OUT
};

enum Dev9Mode : int {
    // Retail behavior
    DEV9_MODE_RETAIL,

    // System 147/148 NAND chip
    DEV9_MODE_NAND,

    // System 246/256 ACATA
    DEV9_MODE_ACATA
};

struct Channel {
    uint32_t madr;
    uint32_t bcr;
    uint32_t chcr;
    uint32_t tadr;
    int transfer_pending;

    // Tag
    uint64_t tag;
    uint32_t addr;
    uint32_t size;
    int irq;
    int eot;
    int extra;
    int32_t transfer_size;

    int adma_remaining;
    int adma_cpb;
};

struct Dma {
    struct {
        bus::Bus* bus;
        iop::intc::Intc* intc;
        sif::Sif* sif;
        cdvd::Cdvd* cdvd;
        ee::dmac::Dmac* ee_dma;
        sio2::Sio2* sio2;
        spu2::Spu2* spu2;
        speed::Speed* speed;
        s2x6::acata::Acata* s2x6_acata;
        scheduler::Scheduler* sched;
        iop::Iop* iop;
    } hw;

    Channel channels[13];

    uint32_t dpcr;
    uint32_t dpcr2;
    uint32_t dicr;
    uint32_t dicr2;
    uint32_t dmacen;
    uint32_t dmacinten;

    int dev9_mode;

    logger::Logger* logger = nullptr;
    size_t logger_id = 0;
};

Dma* create(logger::Logger* logger, iop::intc::Intc* intc, sif::Sif* sif, speed::Speed* speed, scheduler::Scheduler* sched, iop::Iop* iop, bus::Bus* bus);
void connect(Dma* dma, cdvd::Cdvd* cdvd, ee::dmac::Dmac* ee_dma, sio2::Sio2* sio2, spu2::Spu2* spu2, s2x6::acata::Acata* s2x6_acata);
void reset(Dma* dma);
void set_dev9_mode(Dma* dma, int mode);
void destroy(Dma* dma);
uint64_t read16(Dma* dma, uint32_t addr);
void write16(Dma* dma, uint32_t addr, uint64_t data);
uint64_t read32(Dma* dma, uint32_t addr);
void write32(Dma* dma, uint32_t addr, uint64_t data);
void handle_mdec_in_transfer(Dma* dma);
void handle_mdec_out_transfer(Dma* dma);
void handle_sif2_transfer(Dma* dma);
void handle_cdvd_transfer(Dma* dma);
void handle_spu1_transfer(Dma* dma);
void handle_pio_transfer(Dma* dma);
void handle_otc_transfer(Dma* dma);
void handle_spu2_transfer(Dma* dma);
void handle_dev9_transfer(Dma* dma);
void handle_sif0_transfer(Dma* dma);
void handle_sif1_transfer(Dma* dma);
void handle_sio2_in_transfer(Dma* dma);
void handle_sio2_out_transfer(Dma* dma);
void end_sio2_out_transfer(Dma* dma);

void handle_spu1_adma(Dma* dma);
void handle_spu2_adma(Dma* dma);

void end_spu1_transfer(Dma* dma);
void end_spu2_transfer(Dma* dma);

}
