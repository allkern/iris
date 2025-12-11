#include "iris.hpp"

#include "iop/hle/loadcore.h"

#include "res/IconsMaterialSymbols.h"

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

static inline void show_modules_table(iris::instance* iris) {
    using namespace ImGui;

    struct iop_state* iop = iris->ps2->iop;

    if (BeginTable("##iopmodules", 4, ImGuiTableFlags_RowBg | table_sizing)) {
        PushFont(iris->font_small_code);
        TableSetupColumn("Name");
        TableSetupColumn("Text Start");
        TableSetupColumn("Data Start");
        TableSetupColumn("BSS Start");
        TableHeadersRow();
        PopFont();

        for (int i = 0; i < iop->module_count; i++) {
            struct iop_module *mod = &iop->module_list[i];

            TableNextRow();

            TableSetColumnIndex(0);
            Text("%s", mod->name);

            // text section
            TableSetColumnIndex(1);
            
            PushFont(iris->font_code);

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

static inline struct iop_module* find_iop_module(iris::instance* iris, uint32_t addr) {
    struct iop_state* iop = iris->ps2->iop;

    for (int i = 0; i < iop->module_count; i++) {
        struct iop_module* mod = &iop->module_list[i];

        if ((addr >= mod->text_addr) && (addr < (mod->text_addr + mod->text_size)))
            return mod;
    }

    return NULL;
}

void show_iop_modules(iris::instance* iris) {
    using namespace ImGui;

    struct iop_state* iop = iris->ps2->iop;

    if (imgui::BeginEx("IOP Modules", &iris->show_iop_modules, ImGuiWindowFlags_MenuBar)) {
        if (BeginMenuBar()) {
            if (BeginMenu("Settings")) {
                if (BeginMenu(ICON_MS_CROP " Sizing")) {
                    for (int i = 0; i < 4; i++) {
                        if (Selectable(sizing_combo_items[i], i == table_sizing_combo)) {
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
            refresh_module_list(iris->ps2->iop);
        }

        Separator();

        if (BeginChild("##tablechild", ImVec2(0, GetContentRegionAvail().y * 0.9))) {
            show_modules_table(iris);

            EndChild();
        }

        Separator();

        const struct iop_module* mod = find_iop_module(iris, iop->pc);

        if (!mod) {
            Text("Current module: <unknown>\n");
        } else {
            Text("Current module: %s (%08x-%08x)\n", mod->name, mod->text_addr, mod->text_addr + mod->text_size);
        }
    }

    End();
}

}
