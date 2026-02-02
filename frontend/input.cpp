#include <filesystem>
#include <string>

#include "iris.hpp"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

namespace iris {

uint32_t map_keyboard_button(iris::instance* iris, SDL_Keycode k) {
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
    if (k == SDLK_V     ) return DS_BT_ANALOG;

    return 0;
}

static inline input_event sdl_event_to_input_event(SDL_Event* event) {
    input_event ievent = {};

    ievent.type = event->type;

    switch (event->type) {
        case SDL_EVENT_KEY_DOWN:
        case SDL_EVENT_KEY_UP: {
            ievent.id = event->key.key;
        } break;

        case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
        case SDL_EVENT_GAMEPAD_BUTTON_UP: {
            ievent.id = event->gbutton.button;
        } break;

        case SDL_EVENT_GAMEPAD_AXIS_MOTION: {
            ievent.id = event->gaxis.axis;
        } break;
    }

    return ievent;
}

void handle_input_action(iris::instance* iris, input_action act, int slot, uint32_t value) {
    switch (act.action) {
        case INPUT_ACTION_PRESS_BUTTON: {
            if (iris->ds[slot]) {
                ds_button_press(iris->ds[slot], act.destination);
            }
        } break;

        case INPUT_ACTION_RELEASE_BUTTON: {
            if (iris->ds[slot]) {
                ds_button_release(iris->ds[slot], act.destination);
            }
        } break;

        case INPUT_ACTION_MOVE_AXIS: {
            if (iris->ds[slot]) {
                ds_analog_change(iris->ds[slot], act.destination, static_cast<uint8_t>(value));
            }
        } break;
    }
}

void keyboard_device::handle_event(iris::instance* iris, SDL_Event* event) {
    SDL_Keycode key = event->key.key;

    uint32_t mask = map_keyboard_button(iris, key);

    if (event->type == SDL_EVENT_KEY_DOWN) {
        switch (key) {
            case SDLK_I: { if (iris->ds[m_slot]) ds_analog_change(iris->ds[m_slot], DS_AX_LEFT_V, 0x00); } break;
            case SDLK_J: { if (iris->ds[m_slot]) ds_analog_change(iris->ds[m_slot], DS_AX_LEFT_H, 0x00); } break;
            case SDLK_K: { if (iris->ds[m_slot]) ds_analog_change(iris->ds[m_slot], DS_AX_LEFT_V, 0xff); } break;
            case SDLK_L: { if (iris->ds[m_slot]) ds_analog_change(iris->ds[m_slot], DS_AX_LEFT_H, 0xff); } break;
            case SDLK_T: { if (iris->ds[m_slot]) ds_analog_change(iris->ds[m_slot], DS_AX_RIGHT_V, 0x00); } break;
            case SDLK_F: { if (iris->ds[m_slot]) ds_analog_change(iris->ds[m_slot], DS_AX_RIGHT_H, 0x00); } break;
            case SDLK_G: { if (iris->ds[m_slot]) ds_analog_change(iris->ds[m_slot], DS_AX_RIGHT_V, 0xff); } break;
            case SDLK_H: { if (iris->ds[m_slot]) ds_analog_change(iris->ds[m_slot], DS_AX_RIGHT_H, 0xff); } break;
        }

        if (iris->ds[m_slot]) {
            ds_button_press(iris->ds[m_slot], mask);
        }
    } else if (event->type == SDL_EVENT_KEY_UP) {
        switch (key) {
            case SDLK_I: { if (iris->ds[m_slot]) ds_analog_change(iris->ds[m_slot], DS_AX_LEFT_V, 0x7f); } break;
            case SDLK_J: { if (iris->ds[m_slot]) ds_analog_change(iris->ds[m_slot], DS_AX_LEFT_H, 0x7f); } break;
            case SDLK_K: { if (iris->ds[m_slot]) ds_analog_change(iris->ds[m_slot], DS_AX_LEFT_V, 0x7f); } break;
            case SDLK_L: { if (iris->ds[m_slot]) ds_analog_change(iris->ds[m_slot], DS_AX_LEFT_H, 0x7f); } break;
            case SDLK_T: { if (iris->ds[m_slot]) ds_analog_change(iris->ds[m_slot], DS_AX_RIGHT_V, 0x7f); } break;
            case SDLK_F: { if (iris->ds[m_slot]) ds_analog_change(iris->ds[m_slot], DS_AX_RIGHT_H, 0x7f); } break;
            case SDLK_G: { if (iris->ds[m_slot]) ds_analog_change(iris->ds[m_slot], DS_AX_RIGHT_V, 0x7f); } break;
            case SDLK_H: { if (iris->ds[m_slot]) ds_analog_change(iris->ds[m_slot], DS_AX_RIGHT_H, 0x7f); } break;
        }

        if (iris->ds[m_slot]) {
            ds_button_release(iris->ds[m_slot], mask);
        }
    }
}

void gamepad_device::handle_event(iris::instance* iris, SDL_Event* event) {
    if (event->type == SDL_EVENT_GAMEPAD_BUTTON_DOWN || event->type == SDL_EVENT_GAMEPAD_BUTTON_UP) {
        if (event->gbutton.which != id)
            return;

        uint32_t mask = 0;

        switch (event->gbutton.button) {
            case SDL_GAMEPAD_BUTTON_SOUTH:          mask = DS_BT_CROSS; break;
            case SDL_GAMEPAD_BUTTON_EAST:           mask = DS_BT_CIRCLE; break;
            case SDL_GAMEPAD_BUTTON_WEST:           mask = DS_BT_SQUARE; break;
            case SDL_GAMEPAD_BUTTON_NORTH:          mask = DS_BT_TRIANGLE; break;
            case SDL_GAMEPAD_BUTTON_BACK:           mask = DS_BT_SELECT; break;
            case SDL_GAMEPAD_BUTTON_START:          mask = DS_BT_START; break;
            case SDL_GAMEPAD_BUTTON_DPAD_UP:        mask = DS_BT_UP; break;
            case SDL_GAMEPAD_BUTTON_DPAD_DOWN:      mask = DS_BT_DOWN; break;
            case SDL_GAMEPAD_BUTTON_DPAD_LEFT:      mask = DS_BT_LEFT; break;
            case SDL_GAMEPAD_BUTTON_DPAD_RIGHT:     mask = DS_BT_RIGHT; break;
            case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER:  mask = DS_BT_L1; break;
            case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER: mask = DS_BT_R1; break;
            case SDL_GAMEPAD_BUTTON_LEFT_STICK:     mask = DS_BT_L3; break;
            case SDL_GAMEPAD_BUTTON_RIGHT_STICK:    mask = DS_BT_R3; break;
        }

        if (event->type == SDL_EVENT_GAMEPAD_BUTTON_DOWN) {
            if (iris->ds[m_slot]) {
                ds_button_press(iris->ds[m_slot], mask);
            }
        } else if (event->type == SDL_EVENT_GAMEPAD_BUTTON_UP) {
            if (iris->ds[m_slot]) {
                ds_button_release(iris->ds[m_slot], mask);
            }
        }
    } else if (event->type == SDL_EVENT_GAMEPAD_AXIS_MOTION) {
        if (event->gaxis.which != id)
            return;

        // Convert from -32768->32767 to 0.0->1.0
        float value = event->gaxis.value / 32767.0f;

        value = (value + 1.0f) / 2.0f;

        if (iris->ds[m_slot]) {
            switch (event->gaxis.axis) {
                case SDL_GAMEPAD_AXIS_LEFT_TRIGGER: {
                    if (value > 0.5f) {
                        ds_button_press(iris->ds[m_slot], DS_BT_L2);
                    } else {
                        ds_button_release(iris->ds[m_slot], DS_BT_L2);
                    }
                } break;
                case SDL_GAMEPAD_AXIS_RIGHT_TRIGGER: {
                    if (value > 0.5f) {
                        ds_button_press(iris->ds[m_slot], DS_BT_R2);
                    } else {
                        ds_button_release(iris->ds[m_slot], DS_BT_R2);
                    }
                } break;
                case SDL_GAMEPAD_AXIS_LEFTX: {
                    ds_analog_change(iris->ds[m_slot], DS_AX_LEFT_H, (uint8_t)(value * 0xff));
                } break;
                case SDL_GAMEPAD_AXIS_LEFTY: {
                    ds_analog_change(iris->ds[m_slot], DS_AX_LEFT_V, (uint8_t)(value * 0xff));
                } break;
                case SDL_GAMEPAD_AXIS_RIGHTX: {
                    ds_analog_change(iris->ds[m_slot], DS_AX_RIGHT_H, (uint8_t)(value * 0xff));
                } break;
                case SDL_GAMEPAD_AXIS_RIGHTY: {
                    ds_analog_change(iris->ds[m_slot], DS_AX_RIGHT_V, (uint8_t)(value * 0xff));
                } break;
            }
        }
    }
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