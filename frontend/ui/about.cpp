#include <vector>
#include <string>
#include <cctype>

#include "iris.hpp"

#include "res/IconsMaterialSymbols.h"

namespace iris {

void show_about_window(iris::instance* iris) {
    using namespace ImGui;

    static const ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoDocking;

    if (Begin("About", &iris->show_about_window, flags)) {
        Text("Hello, world!");
    } End();
}

}