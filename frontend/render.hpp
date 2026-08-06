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

bool init(Instance* iris);
void destroy(Instance* iris);
bool render_frame(Instance* iris, VkCommandBuffer command_buffer, VkFramebuffer framebuffer);
bool save_screenshot(Instance* iris, std::string path);
void switch_backend(Instance* iris, int backend);
void refresh(Instance* iris);
void gs_dump_start(Instance* iris, std::string path, int frames, int delay, std::string serial);
void gs_dump_tick(Instance* iris);

}

}
