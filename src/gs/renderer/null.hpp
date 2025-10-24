#pragma once

#include <vector>
#include <cstdio>

#include <SDL3/SDL.h>

#include "renderer.hpp"

void* null_create();
bool null_init(void* udata, const renderer_create_info& info);
void null_destroy(void* udata);
renderer_image null_get_frame(void* udata);

extern "C" {
void null_transfer(void* udata, int path, const void* data, size_t size);
}