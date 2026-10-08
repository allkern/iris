#pragma once

#include <atomic>

#include "logger.hpp"
#include "dev/rm.hpp"

namespace iris::speed { struct Speed; }

namespace iris::speed::dvrp {

inline constexpr auto INTR_READY = 0x01;
inline constexpr auto INTR_CMD_ACK = 0x02;
inline constexpr auto INTR_CMD_COMP = 0x04;
inline constexpr auto INTR_DMA_ACK = 0x08;
inline constexpr auto INTR_DMA_END = 0x10;

struct Dvrp {
    // 0x3109 - avioctl2_set_d_audio_sel
    uint16_t cmd;

    // Unknown what these parameters mean
    uint16_t params[0x10];
    uint8_t param_index;

    // bit 1 - Busy
    // bit 2 - DVR/MISC task
    // bit 3 - AV task
    // bit 4 - DVR task
    // bit 5 - IOMAN task
    uint8_t status;

    // bit 7 - busy? (expected to be 0)
    uint8_t status2;

    // DVR INTR regs:
    // 4200 - intr stat (R?)
    // 4204 - intr ack (W?)
    // 4208 - intr mask (RW)
    // 4220 - intr cause/command? (R?)

    // DVR INTRs:
    // bit 0 - DVRRDY (DVR ready)
    // bit 1 - CMD_ACK (Command acknowledged)
    // bit 2 - CMD_COMP (Command completed)
    // bit 3 - DMAACK (DMA transfer acknowledged)
    // bit 4 - DMAEND (DMA transfer ended)
    uint16_t intr_stat; // 4200h
    uint16_t intr_mask; // 4208h
    uint16_t intr_cause; // 4220h Command that caused the interrupt?

    uint16_t reply[64];
    int reply_index;

    std::atomic <dev::rm::Key> remote_key = dev::rm::Key {};
    std::atomic <uint32_t> remote_reads = 0;
    int remote_phase;

    Speed* speed;

    logger::Logger* logger = nullptr;
    size_t logger_id = 0;
};

Dvrp* create(logger::Logger* logger);
void init(Dvrp* dvrp, Speed* speed);
void destroy(Dvrp* dvrp);
uint64_t read(Dvrp* dvrp, uint32_t addr);
void write(Dvrp* dvrp, uint32_t addr, uint64_t data);
void set_remote_key(Dvrp* dvrp, dev::rm::Key key);
void release_remote_key(Dvrp* dvrp);

}
