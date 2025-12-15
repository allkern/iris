#include <filesystem>
#include <string>

#include "iris.hpp"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

namespace iris {

uint32_t map_button(SDL_Keycode k) {
    if (k == SDLK_X     ) return DS_BT_CROSS;
    if (k == SDLK_A     ) return DS_BT_SQUARE;
    if (k == SDLK_W     ) return DS_BT_TRIANGLE;
    if (k == SDLK_D     ) return DS_BT_CIRCLE;
    if (k == SDLK_RETURN) return DS_BT_START;
    if (k == SDLK_S     ) return DS_BT_SELECT;
    if (k == SDLK_UP    ) return DS_BT_UP;
    if (k == SDLK_DOWN  ) return DS_BT_DOWN;
    if (k == SDLK_LEFT  ) return DS_BT_LEFT;
    if (k == SDLK_RIGHT ) return DS_BT_RIGHT;
    if (k == SDLK_Q     ) return DS_BT_L1;
    if (k == SDLK_E     ) return DS_BT_R1;
    if (k == SDLK_1     ) return DS_BT_L2;
    if (k == SDLK_3     ) return DS_BT_R2;
    if (k == SDLK_Z     ) return DS_BT_L3;
    if (k == SDLK_C     ) return DS_BT_R3;

    return 0;
}

std::string get_default_screenshot_filename(iris::instance* iris) {
    SDL_Time t;
    SDL_DateTime dt;

    SDL_GetCurrentTime(&t);
    SDL_TimeToDateTime(t, &dt, true);

    char buf[512];

    sprintf(buf, "Screenshot-%04d-%02d-%02d_%02d-%02d-%02d-%d",
            dt.year, dt.month, dt.day,
            dt.hour, dt.minute, dt.second,
            iris->screenshot_counter + 1
    );

    std::string str(buf);

    switch (iris->screenshot_format) {
        case IRIS_SCREENSHOT_FORMAT_PNG: str += ".png"; break;
        case IRIS_SCREENSHOT_FORMAT_BMP: str += ".bmp"; break;
        case IRIS_SCREENSHOT_FORMAT_JPG: str += ".jpg"; break;
        case IRIS_SCREENSHOT_FORMAT_TGA: str += ".tga"; break;
    }

    return str;
}

int get_screenshot_jpg_quality(iris::instance* iris) {
    switch (iris->screenshot_jpg_quality_mode) {
        case IRIS_SCREENSHOT_JPG_QUALITY_MINIMUM: return 1;
        case IRIS_SCREENSHOT_JPG_QUALITY_LOW:     return 25;
        case IRIS_SCREENSHOT_JPG_QUALITY_MEDIUM:  return 50;
        case IRIS_SCREENSHOT_JPG_QUALITY_HIGH:    return 90;
        case IRIS_SCREENSHOT_JPG_QUALITY_MAXIMUM: return 100;
        case IRIS_SCREENSHOT_JPG_QUALITY_CUSTOM: return iris->screenshot_jpg_quality;
    }

    return 90;
}

bool save_screenshot(iris::instance* iris, std::string path = "") {
    std::filesystem::path fn(path);

    std::string directory = iris->snap_path;
    
    if (iris->snap_path.empty()) {
        directory = "snap";
    }

    std::filesystem::path p(directory);
    std::string absolute_path;
    std::string filename;

    if (path.size()) {
        filename = path;
    } else {
        filename = get_default_screenshot_filename(iris);
    }

    if (p.is_absolute()) {
        absolute_path = p.string();
    } else {
        absolute_path = iris->pref_path + p.string();
    }

    absolute_path += "/" + filename;

    if (fn.is_absolute()) {
        absolute_path = fn.string();
    }

    void* ptr = nullptr;
    int width = 0, height = 0, offset = 0;

    if (iris->screenshot_mode == IRIS_SCREENSHOT_MODE_INTERNAL) {
        ptr = vulkan::read_image(iris,
            iris->image.image,
            iris->image.format,
            iris->image.width,
            iris->image.height
        );

        width = iris->image.width;
        height = iris->image.height;
    } else {
        ptr = vulkan::read_image(iris,
            iris->main_window_data.Frames[0].Backbuffer,
            iris->main_window_data.SurfaceFormat.format,
            iris->main_window_data.Width,
            iris->main_window_data.Height
        );

        width = iris->main_window_data.Width;
        height = iris->main_window_data.Height;
        
        if (!iris->fullscreen) {
            offset = iris->menubar_height;
            height -= iris->menubar_height;
        }
    }

    if (!ptr) {
        push_info(iris, "Couldn't save screenshot");

        return false;
    }

    uint32_t* buf = (uint32_t*)malloc((width * 4) * height);

    memcpy(buf, ((uint32_t*)ptr) + offset * width, (width * 4) * height);

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            buf[x + (y * width)] |= 0xff000000;
        }
    }

    int r = 0;

    switch (iris->screenshot_format) {
        case IRIS_SCREENSHOT_FORMAT_PNG:
            r = stbi_write_png(absolute_path.c_str(), width, height, 4, buf, width * 4);
            break;
        case IRIS_SCREENSHOT_FORMAT_BMP:
            r = stbi_write_bmp(absolute_path.c_str(), width, height, 4, buf);
            break;
        case IRIS_SCREENSHOT_FORMAT_JPG:
            r = stbi_write_jpg(absolute_path.c_str(), width, height, 4, buf, get_screenshot_jpg_quality(iris));
            break;
        case IRIS_SCREENSHOT_FORMAT_TGA:
            r = stbi_write_tga(absolute_path.c_str(), width, height, 4, buf);
            break;
    }

    printf("Saving screenshot to '%s' (%dx%d, %d bpp): %s\n",
           absolute_path.c_str(), width, height, 32, r ? "Success" : "Failure"
    );

    free(ptr);
    free(buf);

    if (!r) {
        push_info(iris, "Couldn't save screenshot");

        return false;
    }

    iris->screenshot_counter++;

    push_info(iris, "Screenshot saved as '" + filename + "'");

    return true;
}

void handle_keydown_event(iris::instance* iris, SDL_KeyboardEvent& key) {
    switch (key.key) {
        case SDLK_SPACE: iris->pause = !iris->pause; break;
        case SDLK_F9: {
            bool saved = save_screenshot(iris);
        } break;
        case SDLK_F11: {
            iris->fullscreen = !iris->fullscreen;

            SDL_SetWindowFullscreen(iris->window, iris->fullscreen ? true : false);
        } break;
        case SDLK_F1: {
            printf("ps2: Sending poweroff signal\n");
            ps2_cdvd_power_off(iris->ps2->cdvd);
        } break;
        case SDLK_0: {
            ps2_iop_intc_irq(iris->ps2->iop_intc, IOP_INTC_USB);
        } break;
        case SDLK_I: ds_analog_change(iris->ds[0], DS_AX_LEFT_V, 0); break;
        case SDLK_J: ds_analog_change(iris->ds[0], DS_AX_LEFT_H, 0); break;
        case SDLK_K: ds_analog_change(iris->ds[0], DS_AX_LEFT_V, 255); break;
        case SDLK_L: ds_analog_change(iris->ds[0], DS_AX_LEFT_H, 255); break;
    }

    uint16_t mask = map_button(key.key);

    ds_button_press(iris->ds[0], mask);
}

void handle_keyup_event(iris::instance* iris, SDL_KeyboardEvent& key) {
    switch (key.key) {
        case SDLK_I: ds_analog_change(iris->ds[0], DS_AX_LEFT_V, 0x7f); break;
        case SDLK_J: ds_analog_change(iris->ds[0], DS_AX_LEFT_H, 0x7f); break;
        case SDLK_K: ds_analog_change(iris->ds[0], DS_AX_LEFT_V, 0x7f); break;
        case SDLK_L: ds_analog_change(iris->ds[0], DS_AX_LEFT_H, 0x7f); break;
    }

    uint16_t mask = map_button(key.key);

    ds_button_release(iris->ds[0], mask);
}

}