#pragma once

#include <cstddef>
#include <cstdint>

#include "intc.hpp"
#include "cdvd.hpp"
#include "scheduler.hpp"
#include "logger.hpp"

namespace iris::rainbow {

inline constexpr auto INTR_ATA0 = 0x0001;
inline constexpr auto INTR_ATA1 = 0x0002;
inline constexpr auto STATUS_ERR = 0x01;
inline constexpr auto STATUS_DRQ = 0x08;
inline constexpr auto STATUS_DSC = 0x10;
inline constexpr auto STATUS_DRDY = 0x40;
inline constexpr auto STATUS_BSY = 0x80;
inline constexpr auto CONTROL_NIEN = 0x02;
inline constexpr auto CONTROL_SRST = 0x04;
inline constexpr auto ERROR_ABRT = 0x04;
inline constexpr auto IREASON_COD = 0x01;
inline constexpr auto IREASON_IO = 0x02;
inline constexpr auto SENSE_NONE = 0x00;
inline constexpr auto SENSE_NOT_READY = 0x02;
inline constexpr auto SENSE_ILLEGAL_REQUEST = 0x05;
inline constexpr auto SENSE_UNIT_ATTENTION = 0x06;

enum Phase {
    PHASE_IDLE,
    PHASE_PACKET,
    PHASE_ATA_DATA_IN,
    PHASE_ATAPI_DATA_IN
};

struct Rainbow {
    struct {
        iop::intc::Intc* intc;
        scheduler::Scheduler* sched;
        cdvd::Cdvd* cdvd;
    } hw;

    int enabled = 0;

    uint16_t dma_ctrl = 0;
    uint16_t intr_stat = 0;
    uint16_t intr_mask = 0;
    uint16_t xfr_ctrl = 0;
    uint16_t if_ctrl = 0;
    uint16_t pio_mode = 0;
    uint16_t mwdma_mode = 0;
    uint16_t udma_mode = 0;
    uint16_t led = 0;
    int irq_line = 0;

    uint8_t error = 0;
    uint8_t feature = 0;
    uint8_t nsector = 0;
    uint8_t sector = 0;
    uint8_t lcyl = 0;
    uint8_t hcyl = 0;
    uint8_t select = 0;
    uint8_t status = 0;
    uint8_t control = 0;

    int phase = PHASE_IDLE;
    uint8_t packet[12] = {};
    int packet_index = 0;

    uint8_t buf[0x10000] = {};
    uint32_t buf_size = 0;
    uint32_t buf_index = 0;
    uint32_t chunk_end = 0;
    uint32_t byte_limit = 0;

    uint8_t sense_key = 0;
    uint8_t asc = 0;
    uint8_t ascq = 0;
    int unit_attention = 0;
    void* last_disc = nullptr;

    logger::Logger* logger = nullptr;
    size_t logger_id = 0;
};

Rainbow* create(logger::Logger* logger, iop::intc::Intc* intc, scheduler::Scheduler* sched, cdvd::Cdvd* cdvd);
void reset(Rainbow* rainbow);
void destroy(Rainbow* rainbow);
void set_enabled(Rainbow* rainbow, int enabled);
uint64_t read8(Rainbow* rainbow, uint32_t addr);
uint64_t read16(Rainbow* rainbow, uint32_t addr);
uint64_t read32(Rainbow* rainbow, uint32_t addr);
void write8(Rainbow* rainbow, uint32_t addr, uint64_t data);
void write16(Rainbow* rainbow, uint32_t addr, uint64_t data);
void write32(Rainbow* rainbow, uint32_t addr, uint64_t data);

}
