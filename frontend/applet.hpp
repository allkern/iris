#pragma once

#include "imgui.h"
#include "imgui.hpp"

namespace iris {

struct Instance;

struct Applet {
    virtual ~Applet() = default;

    virtual void on_init() {}
    virtual void on_open() {}
    virtual void on_render() {}
    virtual void on_close() {}

    virtual bool begin() { return imgui::BeginEx(title, &open, flags); }
    virtual void end() { ImGui::End(); }

    Instance* iris = nullptr;

    const char* id = "";
    const char* title = "";
    ImGuiWindowFlags flags = 0;

    bool open = false;
    bool was_open = false;
    bool persist = true;
};

}
