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
