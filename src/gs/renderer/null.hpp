#pragma once

#include <vector>
#include <cstdio>

#include <SDL3/SDL.h>

#include "renderer.hpp"

namespace iris::gs::renderer::null {

void* create();
bool init(void* udata, const CreateInfo& info);
void reset(void* udata);
void destroy(void* udata);
void set_config(void* udata, void* config);
Image get_frame(void* udata);

void transfer(void* udata, int path, const void* data, size_t size);
void readback(void* udata, void* data, size_t size);
void read_vram(void* udata, void* dst, size_t size);

}
