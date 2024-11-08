#ifndef GS_SOFTWARE_H
#define GS_SOFTWARE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "gs.h"

void gsr_sw_render_point(struct ps2_gs* gs, void* udata);
void gsr_sw_render_line(struct ps2_gs* gs, void* udata);
void gsr_sw_render_line_strip(struct ps2_gs* gs, void* udata);
void gsr_sw_render_triangle(struct ps2_gs* gs, void* udata);
void gsr_sw_render_triangle_strip(struct ps2_gs* gs, void* udata);
void gsr_sw_render_triangle_fan(struct ps2_gs* gs, void* udata);
void gsr_sw_render_sprite(struct ps2_gs* gs, void* udata);
void gsr_sw_render(struct ps2_gs* gs, void* udata);

#ifdef __cplusplus
}
#endif

#endif