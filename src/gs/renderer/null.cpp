#include <cstring>

#include "renderer.hpp"

#include "null.hpp"

namespace iris::gs::renderer::null {

void* create() {
    return nullptr;
}

bool init(void* udata, const CreateInfo& info) {
    return true;
}

void reset(void* udata) {
    // Nothing
}

void destroy(void* udata) {
    // Nothing
}

Image get_frame(void* udata) {
    Image image = {};

    image.image = VK_NULL_HANDLE;
    image.view = VK_NULL_HANDLE;

    return image;
}

void set_config(void* udata, void* config) {
    // Nothing
}


void transfer(void* udata, int path, const void* data, size_t size) {
    // Do nothing
}

void readback(void* udata, void* data, size_t size) {
    // Do nothing
}

void read_vram(void* udata, void* dst, size_t size) {
    memset(dst, 0, size);
}

}
