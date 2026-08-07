#include <vector>
#include <string>
#include <cctype>

#include "iris.hpp"
#include "ee/ee_def.hpp"
#include "ee/vu_def.hpp"

#include "res/IconsMaterialSymbols.h"
#include "imgui_memory_editor.h"
#include "ps2.hpp"

namespace iris {

static MemoryEditor editor;

void MemoryViewer::on_render() {
    using namespace ImGui;

    editor.FontOptions = iris->ui.font_body;

    ps2::Ps2* ps2 = iris->ps2;

    if (BeginTabBar("##tabbar")) {
        if (BeginTabItem("EE")) {
            PushFont(iris->ui.font_code);

            editor.DrawContents(ps2->ee_ram->buf, ps2->ee_ram->size, 0);

            PopFont();

            EndTabItem();
        }

        if (BeginTabItem("EE SPR")) {
            PushFont(iris->ui.font_code);

            editor.DrawContents(ps2->ee->spr->buf, 0x4000, 0);

            PopFont();

            EndTabItem();
        }

        if (BeginTabItem("IOP")) {
            PushFont(iris->ui.font_code);

            editor.DrawContents(ps2->iop_ram->buf, ps2->iop_ram->size, 0);

            PopFont();

            EndTabItem();
        }

        if (BeginTabItem("IOP SPR")) {
            PushFont(iris->ui.font_code);

            editor.DrawContents(ps2->iop_spr->buf, (size_t)ram::Size::_1KB, 0);

            PopFont();

            EndTabItem();
        }

        if (BeginTabItem("VRAM")) {
            PushFont(iris->ui.font_code);

            editor.DrawContents(ps2->gs->vram, 0x400000, 0);

            PopFont();

            EndTabItem();
        }

        if (BeginTabItem("SPU2")) {
            PushFont(iris->ui.font_code);

            editor.DrawContents(ps2->spu2->ram, (size_t)ram::Size::_2MB, 0);

            PopFont();

            EndTabItem();
        }

        if (BeginTabItem("VU0 IMEM")) {
            PushFont(iris->ui.font_code);

            editor.DrawContents(ps2->vu0->micro_mem, 0x1000, 0);

            PopFont();

            EndTabItem();
        }

        if (BeginTabItem("VU0 DMEM")) {
            PushFont(iris->ui.font_code);

            editor.DrawContents(ps2->vu0->vu_mem, 0x1000, 0);

            PopFont();

            EndTabItem();
        }

        if (BeginTabItem("VU1 IMEM")) {
            PushFont(iris->ui.font_code);

            editor.DrawContents(ps2->vu1->micro_mem, 0x4000, 0);

            PopFont();

            EndTabItem();
        }

        if (BeginTabItem("VU1 DMEM")) {
            PushFont(iris->ui.font_code);

            editor.DrawContents(ps2->vu1->vu_mem, 0x4000, 0);

            PopFont();

            EndTabItem();
        }

        if (ps2->s14x_sram) {
            if (BeginTabItem("S14X SRAM")) {
                PushFont(iris->ui.font_code);

                editor.DrawContents(ps2->s14x_sram->buf, 0x8000, 0);

                PopFont();

                EndTabItem();
            }
        }

        if (ps2->s14x_link) {
            if (BeginTabItem("CircLink RAM")) {
                PushFont(iris->ui.font_code);

                editor.DrawContents(ps2->s14x_link->ram, 1024, 0);

                PopFont();

                EndTabItem();
            }
        }

        EndTabBar();
    }
}

}