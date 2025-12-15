#include "renderer.hpp"

#include "null.hpp"

void* null_create() {
    return nullptr;
}

bool null_init(void* udata, const renderer_create_info& info) {
    return true;
}

void null_reset(void* udata) {
    // Nothing
}

void null_destroy(void* udata) {
    // Nothing
}

renderer_image null_get_frame(void* udata) {
    renderer_image image = {};

    image.image = VK_NULL_HANDLE;
    image.view = VK_NULL_HANDLE;

    return image;
}

void null_set_config(void* udata, void* config) {
    // Nothing
}

extern "C" {

void null_transfer(void* udata, int path, const void* data, size_t size) {
    // Do nothing
}

}