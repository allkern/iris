#include <vector>
#include <string>
#include <cctype>

#include "instance.hpp"

#include "res/IconsMaterialSymbols.h"

namespace lunar {

void show_about_window(lunar::instance* lunar) {
    using namespace ImGui;

    static const ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoDocking;

    if (Begin("About", &lunar->show_about_window, flags)) {
        Text("Hello, world!");
    } End();
}

}