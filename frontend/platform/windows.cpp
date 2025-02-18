#include "../platform.hpp"

#include "SDL_syswm.h"
#include <dwmapi.h>

namespace iris {

void init_platform(iris::instance* iris) {
    SDL_SysWMinfo wmInfo;
    SDL_VERSION(&wmInfo.version);
    SDL_GetWindowWMInfo(iris->window, &wmInfo);

    HWND hwnd = wmInfo.info.win.window;

    COLORREF color = 0x00000001;

    DwmSetWindowAttribute(hwnd, 20, &color, sizeof(COLORREF));
}

void destroy_platform(iris::instance* iris) {}

}