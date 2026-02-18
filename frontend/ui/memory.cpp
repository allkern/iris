#include <vector>
#include <string>
#include <cctype>

#include "iris.hpp"
#include "ee/ee_def.hpp"

#include "res/IconsMaterialSymbols.h"
#include "memory_viewer.h"

namespace iris {

static MemoryEditor editor;

void show_memory_viewer(iris::instance* iris) {
    using namespace ImGui;

    editor.FontOptions = iris->font_body;

    struct ps2_state* ps2 = iris->ps2;

    if (imgui::BeginEx("Memory", &iris->show_memory_viewer)) {
        if (BeginTabBar("##tabbar")) {
            if (BeginTabItem("EE")) {
                PushFont(iris->font_code);

                editor.DrawContents(ps2->ee_ram->buf, ps2->ee_ram->size, 0);

                PopFont();

                EndTabItem();
            }

            if (BeginTabItem("EE SPR")) {
                PushFont(iris->font_code);

                editor.DrawContents(ps2->ee->spr->buf, 0x4000, 0);

                PopFont();

                EndTabItem();
            }

            if (BeginTabItem("IOP")) {
                PushFont(iris->font_code);

                editor.DrawContents(ps2->iop_ram->buf, ps2->iop_ram->size, 0);

                PopFont();

                EndTabItem();
            }

            if (BeginTabItem("IOP SPR")) {
                PushFont(iris->font_code);

                editor.DrawContents(ps2->iop_spr->buf, RAM_SIZE_1KB, 0);

                PopFont();

                EndTabItem();
            }

            if (BeginTabItem("VRAM")) {
                PushFont(iris->font_code);

                editor.DrawContents(ps2->gs->vram, 0x400000, 0);

                PopFont();

                EndTabItem();
            }

            if (BeginTabItem("SPU2")) {
                PushFont(iris->font_code);

                editor.DrawContents(ps2->spu2->ram, RAM_SIZE_2MB, 0);

                PopFont();

                EndTabItem();
            }

            if (BeginTabItem("VU0 IMEM")) {
                PushFont(iris->font_code);

                editor.DrawContents(ps2->vu0->micro_mem, 0x1000, 0);

                PopFont();

                EndTabItem();
            }

            if (BeginTabItem("VU0 DMEM")) {
                PushFont(iris->font_code);

                editor.DrawContents(ps2->vu0->vu_mem, 0x1000, 0);

                PopFont();

                EndTabItem();
            }

            if (BeginTabItem("VU1 IMEM")) {
                PushFont(iris->font_code);

                editor.DrawContents(ps2->vu1->micro_mem, 0x4000, 0);

                PopFont();

                EndTabItem();
            }

            if (BeginTabItem("VU1 DMEM")) {
                PushFont(iris->font_code);

                editor.DrawContents(ps2->vu1->vu_mem, 0x4000, 0);

                PopFont();

                EndTabItem();
            }

            EndTabBar();
        }
    } End();
}

}