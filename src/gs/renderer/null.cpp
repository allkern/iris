/*
    This file is part of the Iris PlayStation 2 emulator project.
    Hosted at https://github.com/allkern/iris

    MIT License

    Copyright (c) 2026 Allkern/Lycoder (Lisandro Alarcón)

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in all
    copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
    SOFTWARE.
*/

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

bool get_scanout(void* udata, uint64_t id, Image* image, bool wait) {
    return false;
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
