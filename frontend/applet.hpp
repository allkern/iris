/*
    This file is part of the Iris PlayStation 2 emulator project.
    Hosted at https://github.com/allkern/iris

    MIT License

    Copyright (c) 2026 Allkern/Lycoder (Lisandro Alarcón)

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in all
    copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
    SOFTWARE.
*/

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

    // Now that viewports are the default, we actually don't
    // want to persist most windows, we'll leave it here
    // in case we want to make that configurable.
    bool persist = false;
};

}
