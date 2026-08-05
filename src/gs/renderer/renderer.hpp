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

    void* (*create)();
    bool (*init)(void* udata, const CreateInfo& info);
    void (*reset)(void* udata);
    void (*destroy)(void* udata);
    Image (*get_frame)(void* udata);
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
void read_vram(Renderer* renderer, void* dst, size_t size);

}
