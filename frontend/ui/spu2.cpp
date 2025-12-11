#include <vector>
#include <string>
#include <cctype>

#include "iris.hpp"

#include "res/IconsMaterialSymbols.h"

namespace iris {

int core0_selected = -1;
int core1_selected = -1;

float selected_adsr[120];
int adsr_index;

const char* get_adsr_stage_name(int s) {
    switch (s) {
        case 0: return "Attack";
        case 1: return "Decay";
        case 2: return "Sustain";
        case 3: return "Release";
        case 4: return "End";
    }

    return "None";
}

void show_spu2_core(iris::instance* iris, int c) {
    using namespace ImGui;

    const struct spu2_core* core = &iris->ps2->spu2->c[c];

    bool* mute = c ? iris->core1_mute : iris->core0_mute;
    int* solo = c ? &iris->core1_solo : &iris->core0_solo;
    int* selected = c ? &core1_selected : &core0_selected;

    Text("IRQ address: %08x", core->irqa);

    char id[] = "##spu2coreX";

    id[10] = '0' + c;

    if (BeginTable(id, 7, ImGuiTableFlags_RowBg)) {
        PushFont(iris->font_small_code);
        TableSetupColumn("Voice");
        TableSetupColumn("ENVX");
        TableSetupColumn("NAX");
        TableSetupColumn("SSA");
        TableSetupColumn("LSAX");
        TableSetupColumn("Stage");
        TableSetupColumn("Cycles");
        TableHeadersRow();
        PopFont();

        for (int i = 0; i < 24; i++) {
            TableNextRow();

            TableSetColumnIndex(0);

            char voice[9]; sprintf(voice, "Voice %d", i);

            if (Selectable(voice, i == *selected)) {
                if (i == *selected) {
                    *selected = -1;
                } else {
                    *selected = i;
                }
            }

            TableSetColumnIndex(1);

            PushFont(iris->font_code);
            Text("%04x", core->v[i].envx);

            TableSetColumnIndex(2);

            Text("%08x", core->v[i].nax);
            
            TableSetColumnIndex(3);
            
            Text("%08x", core->v[i].ssa);
            
            TableSetColumnIndex(4);
            
            Text("%08x", core->v[i].lsax);
            
            TableSetColumnIndex(5);
            PopFont();
            Text("%s", get_adsr_stage_name(core->v[i].adsr_phase));
            
            TableSetColumnIndex(6);
            Text("%d", core->v[i].adsr_cycles);
        }

        EndTable();
    }
}

void show_spu2_tab(iris::instance* iris, int c) {
    using namespace ImGui;

    const struct spu2_core* core = &iris->ps2->spu2->c[c];

    show_spu2_core(iris, c);

    Separator();

    int* selected = c ? &core1_selected : &core0_selected;

    if (*selected == -1) {
        Text("No voice selected");

        return;
    }

    if (adsr_index == 119) {
        for (int i = 0; i < 119; i++)
            selected_adsr[i] = selected_adsr[i+1];

        selected_adsr[119] = core->v[*selected].envx / 32767.0f;
    } else {
        selected_adsr[adsr_index++] = core->v[*selected].envx / 32767.0f;
    }

    PlotLines("ADSR", selected_adsr, IM_ARRAYSIZE(selected_adsr), 0, NULL, 0.0f, 1.0f, { 250.0, 80.0 });
}

void show_spu2_debugger(iris::instance* iris) {
    using namespace ImGui;

    const struct ps2_spu2* spu2 = iris->ps2->spu2;

    if (imgui::BeginEx("SPU2", &iris->show_spu2_debugger)) {
        if (BeginTabBar("##spu2tabbar")) {
            if (BeginTabItem("CORE0")) {
                show_spu2_tab(iris, 0);

                EndTabItem();
            }

            if (BeginTabItem("CORE1")) {
                show_spu2_tab(iris, 1);

                EndTabItem();
            }

            EndTabBar();
        }
    } End();
}

}