#pragma once

#include "scheduler.hpp"
#include "logger.hpp"

#include "accore.hpp"

namespace iris::s2x6::acuart {

inline constexpr auto BASE_ADDR = 0x12418000;
inline constexpr auto ADDR_SIZE = 0x1000;

inline constexpr auto R_DATA = 0x00;
inline constexpr auto R_INTERRUPT_ENABLE = 0x02;
inline constexpr auto R_INTERRUPT_ID = 0x04;
inline constexpr auto R_LINE_CONTROL = 0x06;
inline constexpr auto R_MODEM_CONTROL = 0x08;
inline constexpr auto R_LINE_STATUS = 0x0a;
inline constexpr auto R_MODEM_STATUS = 0x0c;
inline constexpr auto R_SCRATCH = 0x0e;

inline constexpr auto LINE_CONTROL_DIVISOR = 0x80;
inline constexpr auto LINE_STATUS_TX_IDLE = 0x60;
inline constexpr auto INTERRUPT_NONE = 0x01;
inline constexpr auto LINE_MAX = 256;
inline constexpr auto LINE_STATUS_RX_READY = 0x01;

enum {
    DEVICE_NONE,
    DEVICE_DRIVE_BOARD
};

inline constexpr auto RX_MAX = 16;
inline constexpr auto STATUS_INTERVAL = 589824;

struct Acuart {
    uint16_t interrupt_enable;
    uint16_t line_control;
    uint16_t modem_control;
    uint16_t scratch;
    uint16_t divisor_low;
    uint16_t divisor_high;

    char line[LINE_MAX];
    int line_size;

    uint8_t rx[RX_MAX];
    int rx_head;
    int rx_size;

    int device;

    accore::Accore* accore;
    scheduler::Scheduler* sched;

    logger::Logger* logger = nullptr;
    size_t logger_id = 0;
};

Acuart* create(logger::Logger* logger, accore::Accore* accore, scheduler::Scheduler* sched);
void set_device(Acuart* acuart, int device);
void destroy(Acuart* acuart);
void reset(Acuart* acuart);
uint64_t read16(Acuart* acuart, uint32_t addr);
void write16(Acuart* acuart, uint32_t addr, uint64_t data);
}
