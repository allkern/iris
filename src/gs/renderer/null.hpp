#pragma once

#include <vector>
#include <cstdio>

#include <SDL3/SDL.h>

void null_init(void* ctx, struct ps2_gs* gs, SDL_Window* window, SDL_GPUDevice* device);
void null_destroy(void* ctx);
void null_set_size(void* ctx, int width, int height);
void null_set_scale(void* ctx, float scale);
void null_set_aspect_mode(void* ctx, int aspect_mode);
void null_set_integer_scaling(void* ctx, bool integer_scaling);
void null_set_bilinear(void* ctx, bool bilinear);
void null_get_viewport_size(void* ctx, int* w, int* h);
void null_get_display_size(void* ctx, int* w, int* h);
void null_get_display_format(void* ctx, int* fmt);
void null_get_interlace_mode(void* ctx, int* mode);
void null_set_window_rect(void* ctx, int x, int y, int w, int h);
void* null_get_buffer_data(void* ctx, int* w, int* h, int* bpp);
renderer_stats* null_get_debug_stats(void* ctx);
const char* null_get_name(void* ctx);
void null_begin_render(void* udata, SDL_GPUCommandBuffer* command_buffer);
void null_render(void* udata, SDL_GPUCommandBuffer* command_buffer, SDL_GPURenderPass* render_pass);
void null_end_render(void* udata, SDL_GPUCommandBuffer* command_buffer);

extern "C" {
void null_render_point(struct ps2_gs* gs, void* udata);
void null_render_line(struct ps2_gs* gs, void* udata);
void null_render_triangle(struct ps2_gs* gs, void* udata);
void null_render_sprite(struct ps2_gs* gs, void* udata);
void null_transfer_start(struct ps2_gs* gs, void* udata);
void null_transfer_write(struct ps2_gs* gs, void* udata);
void null_transfer_read(struct ps2_gs* gs, void* udata);
}