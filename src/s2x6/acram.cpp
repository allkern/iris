#include <new>
#include <cstdlib>

#include "acram.hpp"

namespace iris::s2x6::acram {

Acram* create(logger::Logger* logger, int has_ram) {
    Acram* acram = new Acram();

    acram->logger = logger;
    acram->logger_id = logger::register_source(logger, "acram");

    if (has_ram)
        acram->buf = (uint8_t*)calloc(SIZE, 1);

    return acram;
}

void destroy(Acram* acram) {
    free(acram->buf);

    delete acram;
}

bool is_dma_target(uint32_t target) {
    return (target & 0xff000000) == BASE_ADDR;
}

int bank_from_dma_target(uint32_t target) {
    return ((target - BASE_ADDR) >> BANK_SHIFT) & (NUM_BANKS - 1);
}

uint32_t dma_read32(Acram* acram, int bank) {
    if (!acram->buf)
        return 0;

    uint32_t* addr = &acram->banks[bank].read_addr;
    uint32_t data = 0;

    for (int i = 0; i < 4; i++) {
        data |= acram->buf[*addr & (SIZE - 1)] << (i * 8);

        *addr = (*addr + 1) & (SIZE - 1);
    }

    return data;
}

void dma_write32(Acram* acram, int bank, uint32_t data) {
    if (!acram->buf)
        return;

    uint32_t* addr = &acram->banks[bank].write_addr;

    for (int i = 0; i < 4; i++) {
        acram->buf[*addr & (SIZE - 1)] = (data >> (i * 8)) & 0xff;

        *addr = (*addr + 1) & (SIZE - 1);
    }
}

uint64_t read16(Acram* acram, uint32_t addr) {
    uint32_t offset = addr - BASE_ADDR;
    uint32_t reg = offset & BANK_MASK;

    if (reg < R_STATUS_END)
        return STATUS_READY;

    if (!acram->buf)
        return 0;

    return acram->buf[(offset >> 1) & (SIZE - 1)];
}

void write16(Acram* acram, uint32_t addr, uint64_t data) {
    uint32_t offset = addr - BASE_ADDR;
    uint32_t reg = offset & BANK_MASK;

    int bank = (offset >> BANK_SHIFT) & (NUM_BANKS - 1);

    uint32_t ptr = (bank * BANK_SIZE) + ((uint32_t)(data & 0xffff) << 11) + (reg & 0x7fc);

    if (reg >= R_READ_POINTER && reg < R_WRITE_POINTER) {
        acram->banks[bank].read_addr = ptr;

        return;
    }

    if (reg >= R_WRITE_POINTER && reg < R_POINTER_END) {
        acram->banks[bank].write_addr = ptr;

        return;
    }

    // Config only, the transfer size comes from the DMA channel
    if (reg >= R_CONFIG && reg < R_READ_POINTER)
        return;

    if (!acram->buf)
        return;

    acram->buf[(offset >> 1) & (SIZE - 1)] = data & 0xff;
}
}
