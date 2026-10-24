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

#include <string>
#include <volk.h>

namespace iris {

struct Instance;

namespace render {

enum AspectMode {
    NATIVE,
    STRETCH,
    STRETCH_KEEP,
    FORCE_4_3,
    FORCE_16_9,
    FORCE_5_4,
    AUTO
};

enum ScalingFilter {
    NEAREST,
    BILINEAR,
    FSR,
    CUBIC
};

enum SamplerIndex {
    SAMPLER_NEAREST,
    SAMPLER_BILINEAR,
    SAMPLER_CUBIC
};

enum ScreenshotFormat {
    PNG,
    BMP,
    JPG,
    TGA
};

enum ScreenshotMode {
    INTERNAL,
    DISPLAY
};

enum JpgQuality {
    MINIMUM,
    LOW,
    MEDIUM,
    HIGH,
    MAXIMUM,
    CUSTOM
};

enum PresentMode {
    FPS_30,
    FPS_60,
    VSYNC,
    UNCAPPED
};

enum FramePacing {
    PACING_IMMEDIATE,
    PACING_STEADY
};

bool init(Instance* iris);
void destroy(Instance* iris);
bool render_frame(Instance* iris, VkCommandBuffer command_buffer, VkFramebuffer framebuffer);
bool init_fsr(Instance* iris);
void update_fsr(Instance* iris);
void destroy_fsr(Instance* iris);
VkClearValue background(Instance* iris);
bool save_screenshot(Instance* iris, std::string path);
void switch_backend(Instance* iris, int backend);
void refresh(Instance* iris);
void gs_dump_start(Instance* iris, std::string path, int frames, int delay, std::string serial);
void gs_dump_tick(Instance* iris);

}

}
