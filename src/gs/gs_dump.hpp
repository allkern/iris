#pragma once

#include <cstdint>
#include <cstddef>

#include "gs/gs.hpp"
#include "ee/gif.hpp"

namespace iris::gs::dump {

struct Dump;

Dump* create();
void destroy(Dump* dump);
bool begin(Dump* dump, const char* path, gs::Gs* gs, gif::Gif* gif, const void* vram, uint32_t vram_size, const char* serial, uint32_t crc);
bool is_active(Dump* dump);
void transfer(Dump* dump, int path, const void* data, size_t size);
void vsync(Dump* dump, gs::Gs* gs);
void readfifo(Dump* dump, uint32_t size);
void end(Dump* dump);

}
