#pragma once

#include <vector>
#include <cstdio>

#include <SDL.h>

struct software_state {
    unsigned int sbp, dbp;
    unsigned int sbw, dbw;
    unsigned int spsm, dpsm;
    unsigned int ssax, ssay, dsax, dsay;
    unsigned int rrh, rrw;
    unsigned int dir, xdir;
    unsigned int sx, sy;
    unsigned int dx, dy, px;

    uint32_t psmct24_data;
    uint32_t psmct24_shift;

    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    SDL_Texture* texture = nullptr;
    struct ps2_gs* gs;

    int tex_w = 0;
    int tex_h = 0;
    bool keep_aspect_ratio = true;
    bool integer_scaling = true;
    float scale = 1.5;
};

void software_init(software_state* ctx, struct ps2_gs* gs, SDL_Window* window, SDL_Renderer* renderer);
void software_set_size(software_state* ctx, int width, int height);

extern "C" {
void software_render_point(struct ps2_gs* gs, void* udata);
void software_render_line(struct ps2_gs* gs, void* udata);
void software_render_triangle(struct ps2_gs* gs, void* udata);
void software_render_sprite(struct ps2_gs* gs, void* udata);
void software_render(struct ps2_gs* gs, void* udata);
void software_transfer_start(struct ps2_gs* gs, void* udata);
void software_transfer_write(struct ps2_gs* gs, void* udata);
void software_transfer_read(struct ps2_gs* gs, void* udata);
}