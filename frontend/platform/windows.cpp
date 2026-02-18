#include "iris.hpp"
#include "imgui.h"

#include <dwmapi.h>

namespace iris::platform {

bool init(iris::instance* iris) {
    apply_settings(iris);

    return true;
}

bool apply_settings(iris::instance* iris) {
    SDL_PropertiesID props = SDL_GetWindowProperties(iris->window);

    HWND hwnd = (HWND)SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL);

    COLORREF border_color = iris->windows_enable_borders ? DWMWA_COLOR_DEFAULT : DWMWA_COLOR_NONE;

    if (!SUCCEEDED(DwmSetWindowAttribute(
        hwnd,
        DWMWINDOWATTRIBUTE::DWMWA_BORDER_COLOR,
        &border_color,
        sizeof(border_color)
    ))) {
        return false;
    }

    if (iris->windows_titlebar_style == IRIS_TITLEBAR_DEFAULT) {
        BOOL dark_mode = iris->windows_dark_mode;

        if (!SUCCEEDED(DwmSetWindowAttribute(
            hwnd,
            DWMWINDOWATTRIBUTE::DWMWA_USE_IMMERSIVE_DARK_MODE,
            &dark_mode,
            sizeof(BOOL)
        ))) {
            printf("iris: Failed to set immersive dark mode\n");

            return false;
        }
    }

    bool result = false;

    switch (iris->windows_titlebar_style) {
        case IRIS_TITLEBAR_DEFAULT: {
            COLORREF color = DWMWA_COLOR_DEFAULT;

            result = SUCCEEDED(DwmSetWindowAttribute(
                hwnd,
                DWMWINDOWATTRIBUTE::DWMWA_CAPTION_COLOR,
                &color,
                sizeof(color)
            ));
        } break;

        case IRIS_TITLEBAR_SEAMLESS: {
            COLORREF color = 0;

            ImVec4 menubar_color = ImGui::GetStyleColorVec4(ImGuiCol_MenuBarBg);

            color = (uint32_t)(menubar_color.x * 255) |
                    ((uint32_t)(menubar_color.y * 255) << 8) |
                    ((uint32_t)(menubar_color.z * 255) << 16);

            result = SUCCEEDED(DwmSetWindowAttribute(
                hwnd,
                DWMWINDOWATTRIBUTE::DWMWA_CAPTION_COLOR,
                &color,
                sizeof(color)
            ));
        } break;
    }

    // ShowWindow(hwnd, SW_MINIMIZE);
    // ShowWindow(hwnd, SW_RESTORE);

    return result;
}

void destroy(iris::instance* iris) {}

}