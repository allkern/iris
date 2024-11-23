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
    unsigned int dx, dy;

    SDL_Window* window;
    SDL_Renderer* renderer;
    SDL_Texture* texture;
};

extern "C" {
void software_render_point(struct ps2_gs* gs, void* udata);
void software_render_line(struct ps2_gs* gs, void* udata);
void software_render_line_strip(struct ps2_gs* gs, void* udata);
void software_render_triangle(struct ps2_gs* gs, void* udata);
void software_render_triangle_strip(struct ps2_gs* gs, void* udata);
void software_render_triangle_fan(struct ps2_gs* gs, void* udata);
void software_render_sprite(struct ps2_gs* gs, void* udata);
void software_render(struct ps2_gs* gs, void* udata);
}