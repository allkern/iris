#pragma once
#include "logger.hpp"

namespace iris::speed { struct Speed; }

namespace iris::speed::eeprom {

inline constexpr auto PP_DOUT = 4;
inline constexpr auto PP_DIN = 5;
inline constexpr auto PP_SCLK = 6;
inline constexpr auto PP_CSEL = 7;
inline constexpr auto PP_OP_READ = 2;
inline constexpr auto PP_OP_WRITE = 1;
inline constexpr auto PP_OP_EWEN = 0;
inline constexpr auto PP_OP_EWDS = 0;

enum {
    EEPROM_S_CMD_START,
    EEPROM_S_CMD_READ,
    EEPROM_S_ADDR_READ,
    EEPROM_S_TRANSMIT
};

struct Eeprom {
    int state;

    // Pins
    uint8_t clk;
    uint8_t din;
    uint8_t dout;

    uint8_t cmd;
    uint8_t sequence;
    uint8_t addr;

    uint16_t buf[32];

    logger::Logger* logger = nullptr;
    size_t logger_id = 0;
};

Eeprom* create(logger::Logger* logger);
void init(Eeprom* eeprom);
void load(Eeprom* eeprom, const uint16_t* data);
void destroy(Eeprom* eeprom);
uint64_t read(Eeprom* eeprom);
void write(Eeprom* eeprom, uint64_t data);

}
