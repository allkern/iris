#pragma once

#include <cstdint>

#include "logger.hpp"

namespace iris::vu { struct Vu; }
namespace iris::vif { struct Vif; }
namespace iris::gif { struct Gif; }
namespace iris::gs { struct Gs; }
namespace iris::ee::bus { struct Bus; }

namespace iris::mtvu {

enum Mode : int {
    MODE_OFF = 0,
    MODE_INLINE,
    MODE_THREAD,
    MODE_STRICT
};

enum SyncReason : int {
    SYNC_FRAME,
    SYNC_VU1_MEMORY,
    SYNC_VU1_REGISTERS,
    SYNC_VIF1_ROW,
    SYNC_GS_REGISTERS,
    SYNC_EE_IDLE,
    SYNC_OTHER
};

struct Mtvu;

Mtvu* create(logger::Logger* logger);
void connect(Mtvu* mtvu, vu::Vu* vu0, vu::Vu* vu1, vif::Vif* vif1, gif::Gif* gif, gs::Gs* gs, ee::bus::Bus* bus);
void reset(Mtvu* mtvu);
void destroy(Mtvu* mtvu);

void push_vif_words(Mtvu* mtvu, const uint8_t* data, uint32_t words);
void push_vif_fbrst(Mtvu* mtvu, uint32_t data);
void push_gif_qwords(Mtvu* mtvu, int path, const uint8_t* data, uint32_t qwords);
void push_gif_write32(Mtvu* mtvu, uint32_t addr, uint32_t data);
void push_vu1_execute(Mtvu* mtvu, uint32_t addr);
void push_vu1_reset(Mtvu* mtvu);

void sync(Mtvu* mtvu, SyncReason reason);
void poll(Mtvu* mtvu);

uint32_t read_vif1_row(Mtvu* mtvu, int index);
uint32_t take_gif_fifo_activity(Mtvu* mtvu);
uint64_t get_gif_transfer_hash(Mtvu* mtvu);
void update_gif_backend(Mtvu* mtvu);

}
