#include <new>

#include "acjv.hpp"

namespace iris::s2x6::acjv {

Acjv* create(logger::Logger* logger) {
    Acjv* acjv = new Acjv();

    acjv->logger = logger;
    acjv->logger_id = logger::register_source(logger, "acjv");

    new (acjv) Acjv();

    return acjv;
}

void destroy(Acjv* acjv) {
    delete acjv;
}

void handle_packet(Acjv* acjv, const uint8_t* in, uint8_t* out) {
    JvsPacket* packet = (JvsPacket*)in;

    if (packet->sync != 0x0e) {
        iris_debug(acjv, "Invalid packet sync byte {:02x}", packet->sync);

        return;
    }
}

uint64_t read16(Acjv* acjv, uint32_t addr) {
    if (addr >= RDBASE && addr < 0x124045FE) {
        int x = (addr - RDBASE) / 2;
		if (/*CurrentCMD == NONE &&*/ (x == 2 || x == 3 || x == 4)) return acjv->rdbuf[x] | 1;
        return (uint16_t)acjv->rdbuf[x];
    } else if (addr == 0x124045FE) {
		return (uint16_t)acjv->rdbuf[(addr - RDBASE)/2];
	}

    iris_debug(acjv, "Reading 16-bit value from ACJV at address 0x{:08x}", addr);

    return 0;
}

uint64_t read32(Acjv* acjv, uint32_t addr) {
    iris_fatal_error(acjv, "Reading 32-bit value from ACJV at address 0x{:08x}", addr);

    return 0;
}

void write16(Acjv* acjv, uint32_t addr, uint64_t data) {
    iris_debug(acjv, "Writing 16-bit value to ACJV at address 0x{:08x}", addr);

    if (addr >= WRBASE && addr < 0x12404BFE) { //0x124048FE
        uint32_t x = (addr - WRBASE) / 2;
        acjv->wrbuf[x] = (uint16_t)data;
    } else if (addr == 0x12404BFE) {
        acjv->wrbuf[(addr - WRBASE) / 2] = (uint16_t)data;
    }
}

void write32(Acjv* acjv, uint32_t addr, uint64_t data) {
    iris_debug(acjv, "Writing 32-bit value to ACJV at address 0x{:08x}", addr);
}

}
