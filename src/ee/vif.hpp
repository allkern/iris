#pragma once

#include "u128.h"
#include "bus.hpp"

#include "ee/intc.hpp"
#include "ee/dmac.hpp"
#include "scheduler.hpp"
#include "logger.hpp"

namespace iris::vu { struct Vu; }
namespace iris::ee::dmac { struct Dmac; }
namespace iris::ee::bus { struct Bus; }

namespace iris::vif {

enum {
    VIF_IDLE,
    VIF_RECV_DATA
};

inline constexpr auto ERR_MII = 0x1;
inline constexpr auto STAT_VIS = 1 << 10;
inline constexpr auto STAT_FDR = 1 << 23;
inline constexpr auto VIF0_FIFO_BASE = 0x10004000;
inline constexpr auto VIF1_FIFO_BASE = 0x10005000;
inline constexpr auto STAT_INT = 1 << 11;

inline constexpr auto CMD_NOP = 0x00;
inline constexpr auto CMD_STCYCL = 0x01;
inline constexpr auto CMD_OFFSET = 0x02;
inline constexpr auto CMD_BASE = 0x03;
inline constexpr auto CMD_ITOP = 0x04;
inline constexpr auto CMD_STMOD = 0x05;
inline constexpr auto CMD_MSKPATH3 = 0x06;
inline constexpr auto CMD_MARK = 0x07;
inline constexpr auto CMD_FLUSHE = 0x10;
inline constexpr auto CMD_FLUSH = 0x11;
inline constexpr auto CMD_FLUSHA = 0x13;
inline constexpr auto CMD_MSCAL = 0x14;
inline constexpr auto CMD_MSCALF = 0x15;
inline constexpr auto CMD_MSCNT = 0x17;
inline constexpr auto CMD_STMASK = 0x20;
inline constexpr auto CMD_STROW = 0x30;
inline constexpr auto CMD_STCOL = 0x31;
inline constexpr auto CMD_MPG = 0x4A;
inline constexpr auto CMD_DIRECT = 0x50;
inline constexpr auto CMD_DIRECTHL = 0x51;
// 60h-7Fh UNPACK

inline constexpr auto UNPACK_S_32 = 0;
inline constexpr auto UNPACK_S_16 = 1;
inline constexpr auto UNPACK_S_8 = 2;
inline constexpr auto UNPACK_V2_32 = 4;
inline constexpr auto UNPACK_V2_16 = 5;
inline constexpr auto UNPACK_V2_8 = 6;
inline constexpr auto UNPACK_V3_32 = 8;
inline constexpr auto UNPACK_V3_16 = 9;
inline constexpr auto UNPACK_V3_8 = 10;
inline constexpr auto UNPACK_V4_32 = 12;
inline constexpr auto UNPACK_V4_16 = 13;
inline constexpr auto UNPACK_V4_8 = 14;
inline constexpr auto UNPACK_V4_5 = 15;

struct Vif {
    struct {
        vu::Vu* vu;
        scheduler::Scheduler* sched;
        ee::dmac::Dmac* dmac;
        gif::Gif* gif;
        ee::intc::Intc* intc;
        ee::bus::Bus* bus;
    } hw;

    uint32_t stat;
    uint32_t fbrst;
    uint32_t err;
    uint32_t mark;
    uint32_t cycle;
    uint32_t mode;
    uint32_t num;
    uint32_t mask;
    uint32_t code;
    uint32_t itops;
    uint32_t base;
    uint32_t ofst;
    uint32_t tops;
    uint32_t itop;
    uint32_t top;
    uint32_t r[4];
    uint32_t c[4];

    int dreq;
    int state;
    int pending_words;
    int shift;
    uint32_t cmd;
    uint128_t data;

    uint32_t addr;
    uint32_t unpack_num;
    uint32_t unpack_fmt;
    uint32_t unpack_usn;
    uint32_t unpack_cl;
    uint32_t unpack_wl;
    uint32_t unpack_skip;
    int unpack_wcount;
    uint32_t unpack_buf[16];
    uint32_t unpack_shift;
    uint32_t unpack_data;
    int unpack_mask;
    int unpack_cycle;

    int id;

    logger::Logger* logger = nullptr;
    size_t logger_id = 0;
};

Vif* create(logger::Logger* logger, int id, scheduler::Scheduler* sched, ee::bus::Bus* bus);
void connect(Vif* vif, vu::Vu* vu, gif::Gif* gif, ee::intc::Intc* intc, ee::dmac::Dmac* dmac);
void reset(Vif* vif);
void destroy(Vif* vif);
uint64_t read32(Vif* vif, uint32_t addr);
void write32(Vif* vif, uint32_t addr, uint64_t data);
uint128_t read128(Vif* vif, uint32_t addr);
void write128(Vif* vif, uint32_t addr, uint128_t data);
uint32_t fifo_read(Vif* vif);
void fifo_write(Vif* vif, uint32_t data);

int get_dreq(Vif* vif);

}
