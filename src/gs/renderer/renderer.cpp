#include "renderer.hpp"

#include "null.hpp"
#include "hardware.hpp"

renderer_state* renderer_create(void) {
    return new renderer_state;
}

bool renderer_init(renderer_state* renderer, const renderer_create_info& info) {
    renderer->info = info;

    switch (info.backend) {
        case RENDERER_BACKEND_NULL: {
            renderer->create = null_create;
            renderer->init = null_init;
            renderer->reset = null_reset;
            renderer->destroy = null_destroy;
            renderer->get_frame = null_get_frame;
            renderer->transfer = null_transfer;
        } break;

        case RENDERER_BACKEND_SOFTWARE: {
            // To-do: Software renderer
            renderer->create = null_create;
            renderer->init = null_init;
            renderer->reset = null_reset;
            renderer->destroy = null_destroy;
            renderer->get_frame = null_get_frame;
            renderer->transfer = null_transfer;
        } break;

        case RENDERER_BACKEND_HARDWARE: {
            renderer->create = hardware_create;
            renderer->init = hardware_init;
            renderer->reset = hardware_reset;
            renderer->destroy = hardware_destroy;
            renderer->get_frame = hardware_get_frame;
            renderer->transfer = hardware_transfer;
        } break;
    }

    renderer->udata = renderer->create();

    ps2_gif_set_backend(info.gif, renderer->udata, renderer->transfer);

    return renderer->init(renderer->udata, info);
}

bool renderer_switch(renderer_state* renderer, int backend) {
    if (backend == renderer->info.backend)
        return true;

    renderer->destroy(renderer->udata);

    renderer_create_info info = renderer->info;
    info.backend = backend;

    return renderer_init(renderer, info);
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