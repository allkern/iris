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

#include "iris.hpp"

#include "iop/iop_def.hpp"
#include "iop/hle/loadcore.hpp"

#include "res/IconsMaterialSymbols.h"
#include "ps2.hpp"

namespace iris {

static const char* sizing_combo_items[] = {
    ICON_MS_FIT_WIDTH " Fixed fit",
    ICON_MS_FULLSCREEN " Fixed same",
    ICON_MS_FULLSCREEN " Stretch prop",
    ICON_MS_FULLSCREEN " Stretch same"
};

static ImGuiTableFlags table_sizing_flags[] = {
    ImGuiTableFlags_SizingFixedFit,
    ImGuiTableFlags_SizingFixedSame,
    ImGuiTableFlags_SizingStretchProp,
    ImGuiTableFlags_SizingStretchSame
};

static int table_sizing_combo = 0;
static int table_sizing = ImGuiTableFlags_SizingStretchProp;

static inline void show_modules_table(Instance* iris) {
    using namespace ImGui;

    iop::Iop* iop = iris->ps2->iop;

    if (BeginTable("##iopmodules", 4, ImGuiTableFlags_RowBg | table_sizing)) {
        PushFont(iris->ui.font_small_code);
        TableSetupColumn("Name");
        TableSetupColumn("Text Start");
        TableSetupColumn("Data Start");
        TableSetupColumn("BSS Start");
        TableHeadersRow();
        PopFont();

        for (int i = 0; i < iop->module_count; i++) {
            iop::hle::loadcore::Module *mod = &iop->module_list[i];

            TableNextRow();

            TableSetColumnIndex(0);
            Text("%s", mod->name);

            // text section
            TableSetColumnIndex(1);
            
            PushFont(iris->ui.font_code);

            uint32_t addr = mod->text_addr;
            Text("0x%08x", addr);

            // data section
            TableSetColumnIndex(2);
            addr += mod->text_size;
            Text("0x%08x", addr);

            // bss section
            TableSetColumnIndex(3);
            addr += mod->data_size;
            Text("0x%08x", addr);

            PopFont();
        }

        EndTable();
    }
}

static inline iop::hle::loadcore::Module* find_iop_module(Instance* iris, uint32_t addr) {
    iop::Iop* iop = iris->ps2->iop;

    for (int i = 0; i < iop->module_count; i++) {
        iop::hle::loadcore::Module* mod = &iop->module_list[i];

        if ((addr >= mod->text_addr) && (addr < (mod->text_addr + mod->text_size)))
            return mod;
    }

    return NULL;
}

void IopModules::on_render() {
    using namespace ImGui;

    iop::Iop* iop = iris->ps2->iop;

    if (BeginMenuBar()) {
        if (imgui::BeginMenu("Settings")) {
            if (imgui::BeginMenu(ICON_MS_CROP " Sizing")) {
                for (int i = 0; i < 4; i++) {
                    if (imgui::Selectable(sizing_combo_items[i], i == table_sizing_combo)) {
                        table_sizing = table_sizing_flags[i];
                        table_sizing_combo = i;
                    }
                }

                ImGui::EndMenu();
            }

            ImGui::EndMenu();
        }

        EndMenuBar();
    }

    if (Button(ICON_MS_REFRESH)) {
        iop::hle::loadcore::refresh_module_list(iris->ps2->iop);
    }

    Separator();

    if (BeginChild("##tablechild", ImVec2(0, GetContentRegionAvail().y * 0.9))) {
        show_modules_table(iris);

        EndChild();
    }

    Separator();

    const iop::hle::loadcore::Module* mod = find_iop_module(iris, iop->pc);

    if (!mod) {
        Text("Current module: <unknown>\n");
    } else {
        Text("Current module: %s (%08x-%08x)\n", mod->name, mod->text_addr, mod->text_addr + mod->text_size);
    }
}

}
