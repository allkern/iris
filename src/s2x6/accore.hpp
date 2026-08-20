#pragma once

#include "iop/intc.hpp"
#include "logger.hpp"

#include "acjv.hpp"

namespace iris::s2x6::accore {

inline constexpr auto BASE_ADDR = 0x12415000;
inline constexpr auto ADDR_SIZE = 0xb000;
inline constexpr auto R_CAUSE = 0x1241c000;
inline constexpr auto R_DISABLE_ATA_IRQ = 0x1241510c;
inline constexpr auto R_DISABLE_UART_IRQ = 0x1241511c;
inline constexpr auto R_JVS_STOP = 0x12416000;
inline constexpr auto R_JVS_START = 0x12416002;
inline constexpr auto R_FPGA_PROGRAM_BEGIN = 0x12416008;
inline constexpr auto R_FPGA_PROGRAM_END = 0x12416012;

inline constexpr uint32_t R_IGNORED[] = {
    0x1241511e, 0x12416004, 0x12416006, 0x1241600a, 0x12416014, 0x12416016,
    0x12416018, 0x1241601a, 0x1241601e, 0x12416032, 0x12416036, 0x1241603a,
    0x12417000
};

inline constexpr auto R_ACK_ATA = 0x13000000;
inline constexpr auto R_ACK_UART = 0x13100000;
inline constexpr auto ACK_BASE_ADDR = 0x13000000;
inline constexpr auto ACK_ADDR_SIZE = 0x200000;

enum {
    CAUSE_UART = 0x4000,
    CAUSE_ATA = 0x8000,
    CAUSE_FPGA_BUSY = 0x3000
};

enum {
    IRQ_ATA,
    IRQ_UART
};

struct Accore {
    uint16_t cause;

    int pending[2];

    uint32_t last_unhandled_read;
    uint32_t last_unhandled_write;

    iop::intc::Intc* intc;
    acjv::Acjv* acjv;

    logger::Logger* logger = nullptr;
    size_t logger_id = 0;
};

Accore* create(logger::Logger* logger, iop::intc::Intc* intc, acjv::Acjv* acjv);
void destroy(Accore* accore);
void irq(Accore* accore, int source);
uint64_t read16(Accore* accore, uint32_t addr);
void write16(Accore* accore, uint32_t addr, uint64_t data);
}
