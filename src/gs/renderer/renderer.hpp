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

#pragma once

#include "Granite/vulkan/vulkan_headers.hpp"

#include <volk.h>

#include "gs/gs.hpp"

#include "config.hpp"

namespace iris::gif { struct Gif; }

namespace iris::gs::renderer {

// enum : int {
//     // Keeps aspect ratio by native resolution
//     RENDERER_ASPECT_NATIVE,
//     // Stretch to window (disregard aspect, disregard scale)
//     RENDERER_ASPECT_STRETCH,
//     // Stretch to window (keep aspect, disregard scale)
//     RENDERER_ASPECT_STRETCH_KEEP,
//     // Force 4:3
//     RENDERER_ASPECT_4_3,
//     // Force 16:9
//     RENDERER_ASPECT_16_9,
//     // Force 5:4 (PAL)
//     RENDERER_ASPECT_5_4,
//     // Use NVRAM settings (same as SOFTWARE_ASPECT_STRETCH_KEEP for now)
//     RENDERER_ASPECT_AUTO
// };

enum : int {
    BACKEND_NULL = 0,
    BACKEND_SOFTWARE,
    BACKEND_HARDWARE
};

enum : int {
    PATH_SCANOUT = 0x100
};

struct ScanoutRequest {
    uint64_t id;
    gs::PrivilegedState state;
};

struct Stats {
    unsigned int primitives = 0;
    unsigned int triangles = 0;
    unsigned int lines = 0;
    unsigned int points = 0;
    unsigned int sprites = 0;
    unsigned int texture_uploads = 0;
    unsigned int texture_blits = 0;
    unsigned int frames_rendered = 0;
};
/*
    An Iris renderer consists of two APIs, a backend API that receives
    GIF transfers from the emulation core, and a frontend API that serves
    frames to our frontend

    Backend API
      - render_back_transfer()

    Frontend API
      - render_front_get_frame()
*/

struct CreateInfo {
    gif::Gif* gif;
    gs::Gs* gs;

    VkInstance instance;
    VkInstanceCreateInfo instance_create_info;
    VkDevice device;
    VkDeviceCreateInfo device_create_info;
    VkPhysicalDevice physical_device;
    void* config;

    int backend;
};

struct Image {
    VkImage image;
    VkImageView view;
    VkFormat format;

    unsigned int width;
    unsigned int height;
};

struct Renderer {
    gif::Gif* gif = nullptr;
    void* udata = nullptr;

    CreateInfo info = {};

    int active_backend = BACKEND_NULL;

    void* (*create)();
    bool (*init)(void* udata, const CreateInfo& info);
    void (*reset)(void* udata);
    void (*destroy)(void* udata);
    Image (*get_frame)(void* udata);
    bool (*get_scanout)(void* udata, uint64_t id, Image* image, bool wait);
    void (*transfer)(void* udata, int path, const void* data, size_t size);
    void (*readback)(void* udata, void* data, size_t size);
    void (*read_vram)(void* udata, void* dst, size_t size);
    void (*set_config)(void* udata, void* config);
};

Renderer* create();
bool init(Renderer* renderer, const CreateInfo& info);
bool set_backend(Renderer* renderer, int backend, void* config);
void hotswap(Renderer* renderer, int backend);
void reset(Renderer* renderer);
void destroy(Renderer* renderer);
void set_config(Renderer* renderer, void* config);

Image get_frame(Renderer* renderer);
bool get_scanout(Renderer* renderer, uint64_t id, Image* image, bool wait = true);
void read_vram(Renderer* renderer, void* dst, size_t size);

void lock_queue();
void unlock_queue();

struct QueueLock {
    QueueLock() { lock_queue(); }
    ~QueueLock() { unlock_queue(); }
};

}
