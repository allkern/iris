#pragma once

#include "u128.h"

#include "shared/sif.hpp"

#include "bus_decl.hpp"
#include "ee.hpp"
#include "scheduler.hpp"
#include "logger.hpp"

namespace iris::ipu { class Ipu; }
namespace iris::gif { struct Gif; }
namespace iris::vif { struct Vif; }
namespace iris::vu { struct Vu; }
namespace iris::iop::dma { struct Dma; }
// iop/dma.hpp includes this header in turn

namespace iris::ee::bus { struct Bus; }
namespace iris::ee { struct Ee; }

namespace iris::ee::dmac {

inline uint32_t tag_qwc(uint128_t d)  { return d.u64[0] & 0xffff; }
inline uint32_t tag_pct(uint128_t d)  { return (d.u64[0] >> 26) & 3; }
inline uint32_t tag_id(uint128_t d)   { return (d.u64[0] >> 28) & 7; }
inline uint32_t tag_irq(uint128_t d)  { return (d.u64[0] >> 31) & 1; }
inline uint32_t tag_addr(uint128_t d) { return (d.u64[0] >> 32) & 0xfffffff0; }
inline uint64_t tag_data(uint128_t d) { return d.u64[1]; }

inline constexpr auto VIF0 = 0;
inline constexpr auto VIF1 = 1;
inline constexpr auto GIF = 2;
inline constexpr auto IPU_FROM = 3;
inline constexpr auto IPU_TO = 4;
inline constexpr auto SIF0 = 5;
inline constexpr auto SIF1 = 6;
inline constexpr auto SIF2 = 7;
inline constexpr auto SPR_FROM = 8;
inline constexpr auto SPR_TO = 9;
inline constexpr auto MEIS = 14;

struct Tag {
    uint64_t qwc;
    uint64_t pct;
    uint64_t id;
    uint64_t irq;
    uint64_t addr;
    uint64_t data;
    int end;
};

struct Channel {
    uint32_t chcr;
    uint32_t madr;
    uint32_t tadr;
    uint32_t qwc;
    uint32_t asr0;
    uint32_t asr1;
    uint32_t sadr;

    int dreq;

    // We need to keep track of the current word index
    // for VIF transfers, since the channel can be stopped
    // and restarted in the middle of a qword
    int index;

    Tag tag;
};

struct Dmac {
    // Wiring. Set once by create/connect and preserved across reset.
    struct {
        ee::bus::Bus* bus;
        scheduler::Scheduler* sched;
        sif::Sif* sif;
        ram::Ram* spr;

        gif::Gif* gif;
        vif::Vif* vif0;
        vif::Vif* vif1;
        ipu::Ipu* ipu;
        iop::dma::Dma* iop_dma;
        ee::Ee* ee;
    } hw;

    Channel channels[10];
    Channel* mfifo_drain;

    uint32_t ctrl;
    uint32_t stat;
    uint32_t pcr;
    uint32_t sqwc;
    uint32_t rbsr;
    uint32_t rbor;
    uint32_t enable;

    logger::Logger* logger = nullptr;
    size_t logger_id = 0;
};

Dmac* create(logger::Logger* logger, scheduler::Scheduler* sched, ee::bus::Bus* bus, sif::Sif* sif);

// The DMAC and its peripherals reference each other, so these can only be
// wired once every component exists.
void connect(Dmac* dmac, gif::Gif* gif, vif::Vif* vif0, vif::Vif* vif1, ipu::Ipu* ipu, iop::dma::Dma* iop_dma, ee::Ee* ee);

void reset(Dmac* dmac);
void destroy(Dmac* dmac);
uint64_t read8(Dmac* dmac, uint32_t addr);
uint64_t read16(Dmac* dmac, uint32_t addr);
uint64_t read32(Dmac* dmac, uint32_t addr);
void write8(Dmac* dmac, uint32_t addr, uint64_t data);
void write16(Dmac* dmac, uint32_t addr, uint64_t data);
void write32(Dmac* dmac, uint32_t addr, uint64_t data);

void handle_vif0_transfer(Dmac* dmac);
void handle_vif1_transfer(Dmac* dmac);
void handle_gif_transfer(Dmac* dmac);
void handle_ipu_from_transfer(Dmac* dmac);
void handle_ipu_to_transfer(Dmac* dmac);
void handle_sif0_transfer(Dmac* dmac);
void handle_sif1_transfer(Dmac* dmac);
void handle_sif2_transfer(Dmac* dmac);
void handle_spr_from_transfer(Dmac* dmac);
void handle_spr_to_transfer(Dmac* dmac);

}
