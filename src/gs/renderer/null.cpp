#include "null.hpp"

void null_init(void* ctx, struct ps2_gs* gs, SDL_Window* window) {}
void null_destroy(void* ctx) {}
void null_set_size(void* ctx, int width, int height) {}
void null_set_scale(void* ctx, float scale) {}
void null_set_aspect_mode(void* ctx, int aspect_mode) {}
void null_set_integer_scaling(void* ctx, bool integer_scaling) {}
void null_set_bilinear(void* ctx, bool bilinear) {}
void null_get_viewport_size(void* ctx, int* w, int* h) {
    *w = 0;
    *h = 0;
}
void null_get_display_size(void* ctx, int* w, int* h) {
    *w = 0;
    *h = 0;
}
void null_get_display_format(void* ctx, int* fmt) {
    *fmt = 0;
}
const char* null_get_name(void* ctx) { return "Null"; }

extern "C" void null_render_point(struct ps2_gs* gs, void* udata) {}
extern "C" void null_render_line(struct ps2_gs* gs, void* udata) {}
extern "C" void null_render_triangle(struct ps2_gs* gs, void* udata) {}
extern "C" void null_render_sprite(struct ps2_gs* gs, void* udata) {}
extern "C" void null_render(struct ps2_gs* gs, void* udata) {}
extern "C" void null_transfer_start(struct ps2_gs* gs, void* udata) {}
extern "C" void null_transfer_write(struct ps2_gs* gs, void* udata) {}
extern "C" void null_transfer_read(struct ps2_gs* gs, void* udata) {}