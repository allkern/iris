#include "renderer.hpp"

#include "null.hpp"
#include "hardware.hpp"

renderer_state* renderer_create(void) {
    return new renderer_state;
}

void renderer_init_callbacks(renderer_state* renderer, int backend) {
    switch (backend) {
        case RENDERER_BACKEND_NULL: {
            renderer->create = null_create;
            renderer->init = null_init;
            renderer->reset = null_reset;
            renderer->destroy = null_destroy;
            renderer->get_frame = null_get_frame;
            renderer->set_config = null_set_config;
            renderer->transfer = null_transfer;
            renderer->readback = null_readback;
            renderer->read_vram = null_read_vram;
        } break;

        case RENDERER_BACKEND_SOFTWARE: {
            // To-do: Software renderer
            renderer->create = null_create;
            renderer->init = null_init;
            renderer->reset = null_reset;
            renderer->destroy = null_destroy;
            renderer->get_frame = null_get_frame;
            renderer->set_config = null_set_config;
            renderer->transfer = null_transfer;
            renderer->readback = null_readback;
            renderer->read_vram = null_read_vram;
        } break;

        case RENDERER_BACKEND_HARDWARE: {
            renderer->create = hardware_create;
            renderer->init = hardware_init;
            renderer->reset = hardware_reset;
            renderer->destroy = hardware_destroy;
            renderer->get_frame = hardware_get_frame;
            renderer->set_config = hardware_set_config;
            renderer->transfer = hardware_transfer;
            renderer->readback = hardware_readback;
            renderer->read_vram = hardware_read_vram;
        } break;
    }
}

bool renderer_init(renderer_state* renderer, const renderer_create_info& info) {
    renderer->info = info;

    renderer_init_callbacks(renderer, info.backend);

    renderer->udata = renderer->create();

    ps2_gif_set_backend(info.gif, renderer->udata, renderer->transfer, renderer->readback);

    return renderer->init(renderer->udata, info);
}

bool renderer_switch(renderer_state* renderer, int backend, void* config) {
    if (backend == renderer->info.backend)
        return true;

    renderer->destroy(renderer->udata);

    renderer_create_info info = renderer->info;
    info.backend = backend;
    info.config = config;

    return renderer_init(renderer, info);
}

void renderer_hotswap(renderer_state* renderer, int backend) {
    renderer_init_callbacks(renderer, backend);

    // Re-point the GIF backend at the newly-selected callbacks. Without this the
    // core keeps feeding the previous backend (e.g. hardware) even after a swap
    // to NULL, so GIF transfers would still reach parallel-gs during loading.
    if (renderer->info.gif)
        ps2_gif_set_backend(renderer->info.gif, renderer->udata, renderer->transfer, renderer->readback);
}

void renderer_destroy(renderer_state* renderer) {
    renderer->destroy(renderer->udata);

    delete renderer;
}

void renderer_reset(renderer_state* renderer) {
    renderer->reset(renderer->udata);
}

renderer_image renderer_get_frame(renderer_state* renderer) {
    return renderer->get_frame(renderer->udata);
}

void renderer_read_vram(renderer_state* renderer, void* dst, size_t size) {
    renderer->read_vram(renderer->udata, dst, size);
}

void renderer_set_config(renderer_state* renderer, void* config) {
    renderer->set_config(renderer->udata, config);
}