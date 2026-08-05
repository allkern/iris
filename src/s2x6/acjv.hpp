#pragma once
#include "logger.hpp"

namespace iris::s2x6::acjv {

inline constexpr auto BASE_ADDDR = 0x12400000;
inline constexpr auto RANGE = 0x1240;
inline constexpr auto ADDR_CAP = 0x8000;// ACJV.IRX r/w fun will do `(2 * (addr & 0x3FFF)`, meaning available range is `0x12400000` - `0x12407FFE`
inline constexpr auto PACKETSIZE = 0x300;// seems to be the ammount of bytes that's always read/written by the game

inline constexpr auto RDBASE = 0x12404000;// acmeme access addr 0x2300
inline constexpr auto WRBASE = 0x12404600;// acmeme access addr 0x2000

inline constexpr auto CTR_START = 0x12416002;// set to 0 when ACJV.IRX runs
inline constexpr auto CTR_STOP = 0x12416000;// set to 0 during: JVFIRM upload begins, ACCORE starts, `acJvModuleStop()` is called

inline constexpr auto RDWR_SIZELIMIT = 0x4000;// ACJV.IRX read/write functions only consider 14 bits from addr

inline constexpr auto JVS_CMD_SUCCESS = 0x1;
inline constexpr auto JVS_REVISION = 0x30;//Revision 3.0
inline constexpr auto JVS_VERSION = 0x10;//Version 1.0
inline constexpr auto JVS_PLAYER_COUNT = 2;

struct JvsPacket {
    uint8_t sync;
    uint8_t dest;
    uint8_t size;
    uint8_t data[0x2fc];
    uint8_t checksum;
};

struct Acjv {
    uint16_t wrbuf[PACKETSIZE / 2];
    uint16_t rdbuf[PACKETSIZE / 2];

    uint32_t dummy;

    logger::Logger* logger = nullptr;
    size_t logger_id = 0;
};

Acjv* create(logger::Logger* logger);
void destroy(Acjv* acjv);
uint64_t read16(Acjv* acjv, uint32_t addr);
uint64_t read32(Acjv* acjv, uint32_t addr);
void write16(Acjv* acjv, uint32_t addr, uint64_t data);
void write32(Acjv* acjv, uint32_t addr, uint64_t data);

}
