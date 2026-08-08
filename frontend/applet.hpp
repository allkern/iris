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
    virtual void on_tick() {}

    virtual bool begin() { return imgui::BeginEx(title, &open, flags); }
    virtual void end() { ImGui::End(); }

    void show() {
        open = true;
        focus = true;
    }

    Instance* iris = nullptr;

    const char* id = "";
    const char* title = "";
    ImGuiWindowFlags flags = 0;

    bool needs_ps2 = false;
    bool open = false;
    bool was_open = false;
    bool focus = false;
    bool persist = true;
};

}
