#pragma once

#include <vector>
#include <cstdio>

#include <SDL3/SDL.h>

#include "gs/gs.h"

struct hw_vertex {
    uint32_t r;
    uint32_t g;
    uint32_t b;
    uint32_t a;
    uint32_t u;
    uint32_t v;
    uint32_t x;
    uint32_t y;
    uint32_t z;
    float s;
    float t;
    float q;
};

struct hardware_state {
    SDL_Window* window;
    SDL_GPUDevice* device;
    SDL_GPUGraphicsPipeline* pipeline;
    SDL_GPUBuffer* vertex_buffer;

    struct ps2_gs* gs;

    std::vector <hw_vertex> vertices;

    int tex_w, tex_h;
};

void hardware_init(void* ctx, struct ps2_gs* gs, SDL_Window* window, SDL_GPUDevice* device);
void hardware_destroy(void* ctx);
void hardware_set_size(void* ctx, int width, int height);
void hardware_set_scale(void* ctx, float scale);
void hardware_set_aspect_mode(void* ctx, int aspect_mode);
void hardware_set_integer_scaling(void* ctx, bool integer_scaling);
void hardware_set_bilinear(void* ctx, bool bilinear);
void hardware_get_viewport_size(void* ctx, int* w, int* h);
void hardware_get_display_size(void* ctx, int* w, int* h);
void hardware_get_display_format(void* ctx, int* fmt);
void hardware_get_interlace_mode(void* ctx, int* mode);
void hardware_set_window_rect(void* ctx, int x, int y, int w, int h);
void* hardware_get_buffer_data(void* ctx, int* w, int* h, int* bpp);
const char* hardware_get_name(void* ctx);
void hardware_begin_render(void* udata, SDL_GPUCommandBuffer* command_buffer);
void hardware_render(void* udata, SDL_GPUCommandBuffer* command_buffer, SDL_GPURenderPass* render_pass);
void hardware_end_render(void* udata, SDL_GPUCommandBuffer* command_buffer);

extern "C" {
void hardware_render_point(struct ps2_gs* gs, void* udata);
void hardware_render_line(struct ps2_gs* gs, void* udata);
void hardware_render_triangle(struct ps2_gs* gs, void* udata);
void hardware_render_sprite(struct ps2_gs* gs, void* udata);
void hardware_transfer_start(struct ps2_gs* gs, void* udata);
void hardware_transfer_write(struct ps2_gs* gs, void* udata);
void hardware_transfer_read(struct ps2_gs* gs, void* udata);
}