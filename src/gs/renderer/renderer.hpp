#pragma once

#include "Granite/vulkan/vulkan_headers.hpp"

#include <volk.h>

#include "gs/gs.h"

#include "config.hpp"

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
    RENDERER_BACKEND_NULL = 0,
    RENDERER_BACKEND_SOFTWARE,
    RENDERER_BACKEND_HARDWARE
};

struct renderer_stats {
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

struct renderer_create_info {
    struct ps2_gif* gif;
    struct ps2_gs* gs;

    VkInstance instance;
    VkInstanceCreateInfo instance_create_info;
    VkDevice device;
    VkDeviceCreateInfo device_create_info;
    VkPhysicalDevice physical_device;
    void* config;

    int backend;
};

struct renderer_image {
    VkImage image;
    VkImageView view;
    VkFormat format;

    unsigned int width;
    unsigned int height;
};

struct renderer_state {
    struct ps2_gif* gif = nullptr;
    void* udata = nullptr;

    renderer_create_info info = {};

    void* (*create)();
    bool (*init)(void* udata, const renderer_create_info& info);
    void (*reset)(void* udata);
    void (*destroy)(void* udata);
    renderer_image (*get_frame)(void* udata);
    void (*transfer)(void* udata, int path, const void* data, size_t size);
    void (*set_config)(void* udata, void* config);
};

renderer_state* renderer_create(void);
bool renderer_init(renderer_state* renderer, const renderer_create_info& info);
bool renderer_switch(renderer_state* renderer, int backend, void* config);
void renderer_reset(renderer_state* renderer);
void renderer_destroy(renderer_state* renderer);
void renderer_set_config(renderer_state* renderer, void* config);

renderer_image renderer_get_frame(renderer_state* renderer);