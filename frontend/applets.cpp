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

    ImVec2 size = CalcTextSize(text.c_str());
    ImVec2 avail = GetContentRegionAvail();

    SetCursorPos(ImVec2(
        GetCursorPosX() + std::max(0.0f, (avail.x - size.x) * 0.5f),
        GetCursorPosY() + std::max(0.0f, (avail.y - size.y) * 0.5f)
    ));

    TextDisabled("%s", text.c_str());
}

void create(Instance* iris) {
    Applets& applets = iris->applets;

    applets.all = {
        &applets.ee_control,
        &applets.ee_state,
        &applets.ee_logs,
        &applets.ee_interrupts,
        &applets.ee_dmac,
        &applets.iop_control,
        &applets.iop_state,
        &applets.iop_logs,
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
        &applets.sysmem_logs,
        &applets.console,
        &applets.debugger,
        &applets.memory_card_tool,
        &applets.memory_search,
        &applets.hdd_tool,
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
