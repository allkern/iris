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

#include "applet.hpp"

namespace iris {

struct Debugger : Applet {
    Debugger() {
        id = "debugger";
        title = "Debugger";
        flags = ImGuiWindowFlags_MenuBar;
        needs_ps2 = true;
        persist = false;
    }

    bool begin() override;
    void end() override;
    void on_render() override;

    int cpu = 0;
    int log_source = 0;

    float left_width = 300.0f;
    float right_width = 460.0f;
    float disasm_height = 330.0f;
    float memory_height = 220.0f;
    bool show_left = true;
    bool show_memory = true;
    bool show_logs = true;
    bool memory_open = true;
    bool logs_open = true;
};

}
