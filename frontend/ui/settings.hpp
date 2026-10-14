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

#include <string>

#include "applet.hpp"

namespace iris {

struct SettingsWindow : Applet {
    SettingsWindow() {
        id = "settings";
        title = "Settings";
        flags =
            ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoDocking;
        persist = false;
        needs_ps2 = true;
    }

    bool begin() override;
    void end() override;
    void on_open() override;
    void on_render() override;

    void sync_paths();
    bool paths_dirty() const;

    int selected = 0;

    std::string bios_buf;
    std::string arcade_bios_bufs[emu::ARCADE_BIOS_COUNT];
    std::string rom1_buf;
    std::string rom2_buf;
    std::string nvram_buf;
    std::string hdd_buf;
    std::string flash_buf;
};

}
