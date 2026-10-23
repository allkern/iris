/*
    This file is part of the Iris PlayStation 2 emulator project.
    Hosted at https://github.com/allkern/iris

    MIT License

    Copyright (c) 2026 Allkern/Lycoder (Lisandro Alarcón)

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in all
    copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
    SOFTWARE.
*/

#pragma once

#include <cstdint>

#include "logger.hpp"

namespace iris::vu { struct Vu; }
namespace iris::vif { struct Vif; }
namespace iris::gif { struct Gif; }
namespace iris::gs { struct Gs; }
namespace iris::ee::bus { struct Bus; }

namespace iris::mtvu {

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
void push_backend_packet(Mtvu* mtvu, int path, const void* data, uint32_t bytes);

void sync(Mtvu* mtvu, SyncReason reason);
void poll(Mtvu* mtvu);
void sync_gs_registers(Mtvu* mtvu);
bool worker_events_seen(Mtvu* mtvu);

uint32_t read_vif1_row(Mtvu* mtvu, int index);
uint32_t take_gif_fifo_activity(Mtvu* mtvu);
void update_gif_backend(Mtvu* mtvu);

}
