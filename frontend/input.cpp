#include <filesystem>
#include <string>

#include "iris.hpp"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

namespace iris {

void keyboard_device::handle_event(iris::instance* iris, SDL_Event* event) {
    auto ievent = input::sdl_event_to_input_event(event);
    auto action = input::get_input_action(iris, m_slot, ievent.u64);

    if (!action)
        return;

    input::execute_action(iris, *action, m_slot, event->type == SDL_EVENT_KEY_DOWN ? 1.0f : 0.0f);
}

void gamepad_device::handle_event(iris::instance* iris, SDL_Event* event) {
    auto ievent = input::sdl_event_to_input_event(event);
    auto action = input::get_input_action(iris, m_slot, ievent.u64);

    if (!action)
        return;

    if (event->type == SDL_EVENT_GAMEPAD_BUTTON_DOWN) {
        input::execute_action(iris, *action, m_slot, 1.0f);
    } else if (event->type == SDL_EVENT_GAMEPAD_BUTTON_UP) {
        input::execute_action(iris, *action, m_slot, 0.0f);
    } else if (event->type == SDL_EVENT_GAMEPAD_AXIS_MOTION) {
        // Convert from -32768->32767 to -1.0->1.0 and take absolute value
        float value = fabs(event->gaxis.value / 32767.0f);

        input::execute_action(iris, *action, m_slot, value);
    }
}

}

namespace iris::input {

input_action* get_input_action(iris::instance* iris, int slot, uint64_t input) {
    if (iris->input_map[slot] == -1)
        return nullptr;

    return iris->input_maps[iris->input_map[slot]].map.get_value(input);
}

static inline void change_button(iris::instance* iris, int slot, float value, uint32_t button) {
    if (!iris->ds[slot]) return;

    if (value > 0.5f) {
        ds_button_press(iris->ds[slot], button);
    } else {
        ds_button_release(iris->ds[slot], button);
    }
}

void execute_action(iris::instance* iris, input_action action, int slot, float value) {
    if (!iris->ds[slot])
        return;

    switch (action) {
        case IRIS_DS_BT_SELECT: change_button(iris, slot, value, DS_BT_SELECT); break;
        case IRIS_DS_BT_L3: change_button(iris, slot, value, DS_BT_L3); break;
        case IRIS_DS_BT_R3: change_button(iris, slot, value, DS_BT_R3); break;
        case IRIS_DS_BT_START: change_button(iris, slot, value, DS_BT_START); break;
        case IRIS_DS_BT_UP: change_button(iris, slot, value, DS_BT_UP); break;
        case IRIS_DS_BT_RIGHT: change_button(iris, slot, value, DS_BT_RIGHT); break;
        case IRIS_DS_BT_DOWN: change_button(iris, slot, value, DS_BT_DOWN); break;
        case IRIS_DS_BT_LEFT: change_button(iris, slot, value, DS_BT_LEFT); break;
        case IRIS_DS_BT_L2: change_button(iris, slot, value, DS_BT_L2); break;
        case IRIS_DS_BT_R2: change_button(iris, slot, value, DS_BT_R2); break;
        case IRIS_DS_BT_L1: change_button(iris, slot, value, DS_BT_L1); break;
        case IRIS_DS_BT_R1: change_button(iris, slot, value, DS_BT_R1); break;
        case IRIS_DS_BT_TRIANGLE: change_button(iris, slot, value, DS_BT_TRIANGLE); break;
        case IRIS_DS_BT_CIRCLE: change_button(iris, slot, value, DS_BT_CIRCLE); break;
        case IRIS_DS_BT_CROSS: change_button(iris, slot, value, DS_BT_CROSS); break;
        case IRIS_DS_BT_SQUARE: change_button(iris, slot, value, DS_BT_SQUARE); break;
        case IRIS_DS_BT_ANALOG: change_button(iris, slot, value, DS_BT_ANALOG); break;
        case IRIS_DS_AX_RIGHTV_POS: ds_analog_change(iris->ds[slot], DS_AX_RIGHT_V, 0x7f + (value * 0x80)); break;
        case IRIS_DS_AX_RIGHTV_NEG: ds_analog_change(iris->ds[slot], DS_AX_RIGHT_V, 0x7f - (value * 0x7f)); break;
        case IRIS_DS_AX_RIGHTH_POS: ds_analog_change(iris->ds[slot], DS_AX_RIGHT_H, 0x7f + (value * 0x80)); break;
        case IRIS_DS_AX_RIGHTH_NEG: ds_analog_change(iris->ds[slot], DS_AX_RIGHT_H, 0x7f - (value * 0x7f)); break;
        case IRIS_DS_AX_LEFTV_POS: ds_analog_change(iris->ds[slot], DS_AX_LEFT_V, 0x7f + (value * 0x80)); break;
        case IRIS_DS_AX_LEFTV_NEG: ds_analog_change(iris->ds[slot], DS_AX_LEFT_V, 0x7f - (value * 0x7f)); break;
        case IRIS_DS_AX_LEFTH_POS: ds_analog_change(iris->ds[slot], DS_AX_LEFT_H, 0x7f + (value * 0x80)); break;
        case IRIS_DS_AX_LEFTH_NEG: ds_analog_change(iris->ds[slot], DS_AX_LEFT_H, 0x7f - (value * 0x7f)); break;
    }
}

input_event sdl_event_to_input_event(SDL_Event* event) {
    input_event ievent = {};

    switch (event->type) {
        case SDL_EVENT_KEY_DOWN:
        case SDL_EVENT_KEY_UP: {
            ievent.type = IRIS_EVENT_KEYBOARD;
            ievent.id = event->key.key;
        } break;

        case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
        case SDL_EVENT_GAMEPAD_BUTTON_UP: {
            ievent.type = IRIS_EVENT_GAMEPAD_BUTTON;
            ievent.id = event->gbutton.button;
        } break;

        case SDL_EVENT_GAMEPAD_AXIS_MOTION: {
            if (event->gaxis.value > 0) {
                ievent.type = IRIS_EVENT_GAMEPAD_AXIS_POS;
            } else {
                ievent.type = IRIS_EVENT_GAMEPAD_AXIS_NEG;
            }

            ievent.id = event->gaxis.axis;
        } break;
    }

    return ievent;
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

bool save_screenshot(iris::instance* iris, std::string path) {
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
        renderer_image* image = iris->screenshot_shader_processing ? &iris->output_image : &iris->image;

        ptr = vulkan::read_image(iris,
            image->image,
            image->format,
            image->width,
            image->height
        );

        width = image->width;
        height = image->height;
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

void handle_keydown_event(iris::instance* iris, SDL_Event* event) {
    SDL_Keycode key = event->key.key;

    switch (key) {
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
    }

    iris->last_input_event_read = false;
    iris->last_input_event_value = 1.0f;
    iris->last_input_event = sdl_event_to_input_event(event);

    if (iris->input_devices[0]) iris->input_devices[0]->handle_event(iris, event);
    if (iris->input_devices[1]) iris->input_devices[1]->handle_event(iris, event);
}

void handle_keyup_event(iris::instance* iris, SDL_Event* event) {
    SDL_Keycode key = event->key.key;

    // Add special keyup handling here if needed

    if (iris->input_devices[0]) iris->input_devices[0]->handle_event(iris, event);
    if (iris->input_devices[1]) iris->input_devices[1]->handle_event(iris, event);
}

}