#include <vector>
#include <string>
#include <cctype>

#include "iris.hpp"
#include "ee/ee_def.hpp"
#include "ee/vu_def.hpp"

#include "res/IconsMaterialSymbols.h"
#include "imgui_memory_editor.h"
#include "portable-file-dialogs.h"
#include "ps2.hpp"

namespace iris {

static MemoryEditor editor;
static const char* selected_label = nullptr;
static void* selected_buf = nullptr;
static size_t selected_size = 0;

static std::string dump_file_name(const char* label) {
    std::string name;

    for (const char* c = label; *c; c++) {
        name += *c == ' ' ? '_' : (char)std::tolower((unsigned char)*c);
    }

    return name + ".bin";
}

static void save_memory(const char* label, void* buf, size_t size) {
    pfd::save_file file(std::string("Save ") + label, dump_file_name(label), {
        "Binary files (*.bin)", "*.bin",
        "All files (*.*)", "*"
    });

    if (file.result().empty())
        return;

    FILE* f = fopen(file.result().c_str(), "wb");

    if (!f) {
        pfd::message("Error", "Failed to open file for writing.", pfd::choice::ok, pfd::icon::error);

        return;
    }

    fwrite(buf, 1, size, f);
    fclose(f);
}

void MemoryViewer::on_render() {
    using namespace ImGui;

    editor.FontOptions = iris->ui.font_body;

    ps2::Ps2* ps2 = iris->ps2;

    if (BeginMenuBar()) {
        if (BeginMenu("File")) {
            if (imgui::MenuItem(ICON_MS_SAVE " Save", nullptr, false, selected_buf && selected_size)) {
                save_memory(selected_label, selected_buf, selected_size);
            }

            ImGui::EndMenu();
        }

        EndMenuBar();
    }

    auto draw_memory_tab = [&](const char* label, void* buf, size_t size) {
        if (BeginTabItem(label)) {
            selected_label = label;
            selected_buf = buf;
            selected_size = size;

            PushFont(iris->ui.font_code);

            editor.DrawContents(buf, size, 0);

            PopFont();

            EndTabItem();
        }
    };

    if (BeginTabBar("##tabbar")) {
        draw_memory_tab("EE", ps2->ee_ram->buf, ps2->ee_ram->size);
        draw_memory_tab("EE SPR", ps2->ee->spr->buf, 0x4000);
        draw_memory_tab("IOP", ps2->iop_ram->buf, ps2->iop_ram->size);
        draw_memory_tab("IOP SPR", ps2->iop_spr->buf, ps2->iop_spr->size);
        draw_memory_tab("VRAM", ps2->gs->vram, 0x400000);
        draw_memory_tab("SPU2", ps2->spu2->ram, 0x200000);
        draw_memory_tab("VU0 IMEM", ps2->vu0->micro_mem, 0x1000);
        draw_memory_tab("VU0 DMEM", ps2->vu0->vu_mem, 0x1000);
        draw_memory_tab("VU1 IMEM", ps2->vu1->micro_mem, 0x4000);
        draw_memory_tab("VU1 DMEM", ps2->vu1->vu_mem, 0x4000);

        if (ps2->s14x_sram) {
            draw_memory_tab("S14X SRAM", ps2->s14x_sram->buf, 0x8000);
        }

        if (ps2->s14x_link) {
            draw_memory_tab("CircLink RAM", ps2->s14x_link->ram, 1024);
        }

        EndTabBar();
    }
}

}
