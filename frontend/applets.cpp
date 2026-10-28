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

#include <algorithm>
#include <string>

#include "iris.hpp"
#include "applets.hpp"

#include "res/IconsMaterialSymbols.h"

namespace iris {

namespace applets {

static void show_loading(Instance* iris) {
    using namespace ImGui;

    std::string text = ICON_MS_HOURGLASS_TOP " Loading";

    if (iris->ui.loading_target.size())
        text += " " + iris->ui.loading_target;

    text += "...";

    imgui::TextDisabledCentered("%s", text.c_str());
}

void create(Instance* iris) {
    Applets& applets = iris->applets;

    applets.all = {
        &applets.ee_control,
        &applets.ee_state,
        &applets.ee_interrupts,
        &applets.ee_dmac,
        &applets.iop_control,
        &applets.iop_state,
        &applets.iop_interrupts,
        &applets.iop_modules,
        &applets.iop_dma,
        &applets.gs_debugger,
        &applets.spu2_debugger,
        &applets.memory_viewer,
        &applets.vu_disassembler,
        &applets.breakpoints,
        &applets.about,
        &applets.compat_report,
        &applets.settings,
        &applets.pad_debugger,
        &applets.dvd_remote,
        &applets.symbols,
        &applets.ee_threads,
        &applets.iop_threads,
        &applets.timers,
        &applets.logs,
        &applets.console,
        &applets.debugger,
        &applets.media_tool,
        &applets.memory_search,
        &applets.file_explorer,
        &applets.script_editor,
        &applets.gs_dump_tool,
        &applets.bios_setting
    };

    for (Applet* a : applets.all)
        a->iris = iris;
}

void init(Instance* iris) {
    for (Applet* a : iris->applets.all)
        a->on_init();
}

void render(Instance* iris) {
    for (Applet* a : iris->applets.all) {
        a->on_tick();

        if (a->open && !a->was_open) {
            a->was_open = true;

            a->on_open();
        }

        if (a->open) {
            if (a->focus) {
                ImGui::SetNextWindowFocus();
            }

            bool visible = a->begin();

            if (a->focus) {
                ImGuiViewport* viewport = ImGui::GetWindowViewport();

                if (viewport == ImGui::GetMainViewport()) {
                    a->focus = false;
                } else if (viewport && viewport->PlatformUserData && ImGui::GetPlatformIO().Platform_SetWindowFocus) {
                    ImGui::GetPlatformIO().Platform_SetWindowFocus(viewport);

                    a->focus = false;
                }
            }

            if (visible) {
                if (a->needs_ps2 && iris->ui.loading_file_active) {
                    show_loading(iris);
                } else {
                    a->on_render();
                }
            }

            a->end();
        }

        if (!a->open && a->was_open) {
            a->was_open = false;

            a->on_close();
        }
    }
}

}

}
