#include <math.h>

#include "gs_software.h"

void gsr_sw_render_point(struct ps2_gs* gs, void* udata) {
    struct gs_vertex v = gs->vq[0];

    int x = (v.xyz2 & 0xffff) >> 4;
    int y = (v.xyz2 & 0xffff0000) >> 20;
    int z = v.xyz2 >> 32;
    int w = ((gs->frame_1 >> 16) & 0x3f) * 64;

    gs->vram[x + (y * w)] = v.rgbaq;
}
void gsr_sw_render_line(struct ps2_gs* gs, void* udata) {}
void gsr_sw_render_line_strip(struct ps2_gs* gs, void* udata) {}
void gsr_sw_render_triangle(struct ps2_gs* gs, void* udata) {
    struct {
        float x, y, z
    } a, b, c;

    a.x = (gs->vq[0].xyz2 & 0xffff) >> 4;
    a.y = (gs->vq[0].xyz2 & 0xffff0000) >> 20;
    a.z = gs->vq[0].xyz2 >> 32;
    b.x = (gs->vq[1].xyz2 & 0xffff) >> 4;
    b.y = (gs->vq[1].xyz2 & 0xffff0000) >> 20;
    b.z = gs->vq[1].xyz2 >> 32;
    c.x = (gs->vq[2].xyz2 & 0xffff) >> 4;
    c.y = (gs->vq[2].xyz2 & 0xffff0000) >> 20;
    c.z = gs->vq[2].xyz2 >> 32;
    int w = ((gs->frame_1 >> 16) & 0x3f) * 64;

    int xmin = fmin(fmin(a.x, b.x), c.x);
    int ymin = fmin(fmin(a.y, b.y), c.y);
    int xmax = fmax(fmax(a.x, b.x), c.x);
    int ymax = fmax(fmax(a.y, b.y), c.y);

    for (int y = ymin; y < ymax; y++) {
        for (int x = xmin; x < xmax; x++) {
            int z0 = ((b.x - a.x) * (y - a.y)) - ((b.y - a.y) * (x - a.x));
            int z1 = ((c.x - b.x) * (y - b.y)) - ((c.y - b.y) * (x - b.x));
            int z2 = ((a.x - c.x) * (y - c.y)) - ((a.y - c.y) * (x - c.x));

            if ((z0 >= 0) && (z1 >= 0) && (z2 >= 0)) {
                gs->vram[x + (y * w)] = 0xff0000ff;
            }
        }
    }
}
void gsr_sw_render_triangle_strip(struct ps2_gs* gs, void* udata) {}
void gsr_sw_render_triangle_fan(struct ps2_gs* gs, void* udata) {}
void gsr_sw_render_sprite(struct ps2_gs* gs, void* udata) {}
void gsr_sw_render(struct ps2_gs* gs, void* udata) {}