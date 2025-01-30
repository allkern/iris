#include <vector>
#include <string>
#include <cctype>

#include "instance.hpp"

#include "res/IconsMaterialSymbols.h"

#include "ee/ee_dis.h"
#include "iop/iop_dis.h"

#define IM_RGB(r, g, b) ImVec4(((float)r / 255.0f), ((float)g / 255.0f), ((float)b / 255.0f), 1.0)

namespace lunar {

static const char* ee_cop0_r[] = {
    "Index",
    "Random",
    "EntryLo0",
    "EntryLo1",
    "Context",
    "PageMask",
    "Wired",
    "Unused7",
    "BadVAddr",
    "Count",
    "EntryHi",
    "Compare",
    "Status",
    "Cause",
    "EPC",
    "PRId",
    "Config",
    "Unused17",
    "Unused18",
    "Unused19",
    "Unused20",
    "Unused21",
    "Unused22",
    "BadPAddr",
    "Debug",
    "Perf",
    "Unused26",
    "Unused27",
    "TagLo",
    "TagHi",
    "ErrorEPC",
    "Unused31"
};

static const char* mips_cc_r[] = {
    "r0", "at", "v0", "v1", "a0", "a1", "a2", "a3",
    "t0", "t1", "t2", "t3", "t4", "t5", "t6", "t7",
    "s0", "s1", "s2", "s3", "s4", "s5", "s6", "s7",
    "t8", "t9", "k0", "k1", "gp", "sp", "fp", "ra"
};

uint128_t ee_prev[32];
uint128_t ee_frames[32];
uint32_t ee_cop0_prev[32];
uint32_t ee_cop0_frames[32];
uint32_t ee_fpu_prev[32];
uint32_t ee_fpu_frames[32];
uint32_t iop_prev[32];
uint32_t iop_frames[32];

static ImGuiTableFlags ee_table_sizing = ImGuiTableFlags_SizingStretchSame;
static ImGuiTableFlags iop_table_sizing = ImGuiTableFlags_SizingStretchProp;

static inline void show_ee_main_registers(lunar::instance* lunar) {
    using namespace ImGui;

    struct ee_state* ee = lunar->ps2->ee;

    for (int i = 0; i < 32; i++) {
        for (int j = 0; j < 4; j++) {
            if (ee_prev[i].u32[j] != ee->r[i].u32[j])
                ee_frames[i].u32[j] = 60;
        }

        ee_prev[i] = ee->r[i];
    }

    PushFont(lunar->font_code);

    if (BeginTable("ee#registers", 5, ImGuiTableFlags_RowBg | ee_table_sizing)) {
        PushFont(lunar->font_small_code);
        TableSetupColumn("Reg");
        TableSetupColumn("96-127");
        TableSetupColumn("64-95");
        TableSetupColumn("32-63");
        TableSetupColumn("0-31");
        TableHeadersRow();
        PopFont();

        for (int i = 0; i < 32; i++) {
            TableNextRow();

            TableSetColumnIndex(0);

            Text("%s", mips_cc_r[i]);

            for (int j = 0; j < 4; j++) {
                float a = (float)ee_frames[i].u32[j] / 60.0f;

                TableSetColumnIndex(1+(3-j));

                char label[5]; sprintf(label, "##%02x", (i << 2) | j);

                if (Selectable(label, false, ImGuiSelectableFlags_AllowOverlap)) {
                    OpenPopup("Change register value");
                } SameLine(0.0, 0.0);

                if (BeginPopupContextItem(NULL, ImGuiPopupFlags_MouseButtonLeft)) {
                    static char new_value[9];

                    PushFont(lunar->font_small_code);
                    TextDisabled("Edit "); SameLine(0.0, 0.0);
                    Text("%s", mips_cc_r[i]); SameLine(0.0, 0.0);
                    TextDisabled(" (bits %d-%d)", j*32, ((j+1)*32)-1);
                    PopFont();

                    PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6.0, 2.0));
                    PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0, 8.0));

                    PushFont(lunar->font_body);
                    PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.35, 0.35, 0.35, 0.35));

                    AlignTextToFramePadding();
                    Text(ICON_MS_EDIT); SameLine();

                    SetNextItemWidth(100);
                    PushFont(lunar->font_code);

                    if (InputText("##", new_value, 9, ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_EnterReturnsTrue)) {
                        if (new_value[0])
                            ee->r[i].u32[j] = strtoul(new_value, NULL, 16);

                        CloseCurrentPopup();
                    }

                    PopFont();

                    if (Button("Change")) {
                        if (new_value[0])
                            ee->r[i].u32[j] = strtoul(new_value, NULL, 16);

                        CloseCurrentPopup();
                    } SameLine();

                    if (Button("Cancel"))
                        CloseCurrentPopup();

                    PopStyleColor();
                    PopStyleVar(2);

                    PopFont();
                    EndPopup();
                }

                TextColored(ImVec4(0.6+a, 0.6, 0.6, 1.0), "%08x", ee->r[i].u32[j]);
            }
        }
    }

    EndTable();

    PopFont();
}

static inline void show_ee_cop0_registers(lunar::instance* lunar) {
    using namespace ImGui;

    struct ee_state* ee = lunar->ps2->ee;

    for (int i = 0; i < 32; i++) {
        if (ee_cop0_prev[i] != ee->cop0_r[i])
            ee_cop0_frames[i] = 60;

        ee_cop0_prev[i] = ee->cop0_r[i];
    }

    PushFont(lunar->font_code);

    if (BeginTable("ee#cop0registers", 5, ImGuiTableFlags_RowBg | ee_table_sizing)) {
        PushFont(lunar->font_small_code);
        TableSetupColumn("Reg");
        TableSetupColumn("96-127");
        TableSetupColumn("64-95");
        TableSetupColumn("32-63");
        TableSetupColumn("0-31");
        TableHeadersRow();
        PopFont();

        for (int i = 0; i < 32; i++) {
            float a = (float)ee_cop0_frames[i] / 60.0f;

            TableNextRow();

            TableSetColumnIndex(0);

            Text("%s", ee_cop0_r[i]);

            TableSetColumnIndex(4);

            char label[8]; sprintf(label, "##e%d", (i << 2));

            if (Selectable(label, false, ImGuiSelectableFlags_AllowOverlap)) {
                OpenPopup("Change register value");
            } SameLine(0.0, 0.0);

            if (BeginPopupContextItem(NULL, ImGuiPopupFlags_MouseButtonLeft)) {
                static char new_value[9];

                PushFont(lunar->font_small_code);
                TextDisabled("Edit "); SameLine(0.0, 0.0);
                Text("%s", ee_cop0_r[i]);
                PopFont();

                PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6.0, 2.0));
                PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0, 8.0));

                PushFont(lunar->font_body);
                PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.35, 0.35, 0.35, 0.35));

                AlignTextToFramePadding();
                Text(ICON_MS_EDIT); SameLine();

                SetNextItemWidth(100);
                PushFont(lunar->font_code);

                if (InputText("##", new_value, 9, ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_EnterReturnsTrue)) {
                    if (new_value[0])
                        ee->cop0_r[i] = strtoul(new_value, NULL, 16);

                    CloseCurrentPopup();
                }

                PopFont();

                if (Button("Change")) {
                    if (new_value[0])
                        ee->cop0_r[i] = strtoul(new_value, NULL, 16);

                    CloseCurrentPopup();
                } SameLine();

                if (Button("Cancel"))
                    CloseCurrentPopup();

                PopStyleColor();
                PopStyleVar(2);

                PopFont();
                EndPopup();
            }

            TextColored(ImVec4(0.6+a, 0.6, 0.6, 1.0), "%08x", ee->cop0_r[i]);
        }
    }

    EndTable();

    PopFont();
}

static inline void show_ee_fpu_registers(lunar::instance* lunar) {
using namespace ImGui;

    struct ee_state* ee = lunar->ps2->ee;

    for (int i = 0; i < 32; i++) {
        if (ee_fpu_prev[i] != ee->f[i].u32)
            ee_fpu_frames[i] = 60;

        ee_fpu_prev[i] = ee->f[i].u32;
    }

    PushFont(lunar->font_code);

    if (BeginTable("ee#fpuregisters", 3, ImGuiTableFlags_RowBg | ee_table_sizing)) {
        PushFont(lunar->font_small_code);
        TableSetupColumn("Reg");
        TableSetupColumn("u32");
        TableSetupColumn("float");
        TableHeadersRow();
        PopFont();

        for (int i = 0; i < 32; i++) {
            float a = (float)ee_fpu_frames[i] / 60.0f;

            TableNextRow();

            TableSetColumnIndex(0);

            Text("f%-2d", i);

            TableSetColumnIndex(1);

            char label1[8]; sprintf(label1, "##u%d", i);

            if (Selectable(label1, false, ImGuiSelectableFlags_AllowOverlap)) {
                OpenPopup("Change register value");
            } SameLine(0.0, 0.0);

            if (BeginPopupContextItem(NULL, ImGuiPopupFlags_MouseButtonLeft)) {
                static char new_value[9];

                PushFont(lunar->font_small_code);
                TextDisabled("Edit "); SameLine(0.0, 0.0);
                Text("f%d ", i); SameLine(0.0, 0.0);
                TextDisabled("(as u32)");
                PopFont();

                PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6.0, 2.0));
                PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0, 8.0));

                PushFont(lunar->font_body);
                PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.35, 0.35, 0.35, 0.35));

                AlignTextToFramePadding();
                Text(ICON_MS_EDIT); SameLine();

                SetNextItemWidth(100);
                PushFont(lunar->font_code);

                if (InputText("##", new_value, 9, ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_EnterReturnsTrue)) {
                    if (new_value[0])
                        ee->f[i].u32 = strtoul(new_value, NULL, 16);

                    CloseCurrentPopup();
                }

                PopFont();

                if (Button("Change")) {
                    if (new_value[0])
                        ee->f[i].u32 = strtoul(new_value, NULL, 16);

                    CloseCurrentPopup();
                } SameLine();

                if (Button("Cancel"))
                    CloseCurrentPopup();

                PopStyleColor();
                PopStyleVar(2);

                PopFont();
                EndPopup();
            }

            TextColored(ImVec4(0.6+a, 0.6, 0.6, 1.0), "%08x", ee->f[i].u32);

            TableSetColumnIndex(2);

            char label2[8]; sprintf(label2, "##f%d", i);

            if (Selectable(label2, false, ImGuiSelectableFlags_AllowOverlap)) {
                OpenPopup("Change register value");
            } SameLine(0.0, 0.0);

            if (BeginPopupContextItem(NULL, ImGuiPopupFlags_MouseButtonLeft)) {
                static char new_value[9];

                PushFont(lunar->font_small_code);
                TextDisabled("Edit "); SameLine(0.0, 0.0);
                Text("f%d ", i); SameLine(0.0, 0.0);
                TextDisabled("(as float)");
                PopFont();

                PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6.0, 2.0));
                PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0, 8.0));

                PushFont(lunar->font_body);
                PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.35, 0.35, 0.35, 0.35));

                AlignTextToFramePadding();
                Text(ICON_MS_EDIT); SameLine();

                SetNextItemWidth(100);
                PushFont(lunar->font_code);

                if (InputText("##", new_value, 9, ImGuiInputTextFlags_CharsScientific | ImGuiInputTextFlags_EnterReturnsTrue)) {
                    if (new_value[0])
                        ee->f[i].f = strtof(new_value, NULL);

                    CloseCurrentPopup();
                }

                PopFont();

                if (Button("Change")) {
                    if (new_value[0])
                        ee->f[i].f = strtof(new_value, NULL);

                    CloseCurrentPopup();
                } SameLine();

                if (Button("Cancel"))
                    CloseCurrentPopup();

                PopStyleColor();
                PopStyleVar(2);

                PopFont();
                EndPopup();
            }

            TextColored(ImVec4(0.6+a, 0.6, 0.6, 1.0), "%.7f", ee->f[i].f);
        }
    }

    EndTable();

    PopFont();
}

static inline void show_iop_main_registers(lunar::instance* lunar) {
    using namespace ImGui;

    PushFont(lunar->font_code);

    struct iop_state* iop = lunar->ps2->iop;

    for (int i = 0; i < 32; i++) {
        if (iop_prev[i] != iop->r[i])
            iop_frames[i] = 60;

        iop_prev[i] = iop->r[i];
    }

    if (BeginTable("table3", 4, ImGuiTableFlags_RowBg | iop_table_sizing)) {
        for (int i = 0; i < 32;) {
            TableNextRow();

            for (int x = 0; (x < 4) && (i < 32); x++) {
                float a = (float)iop_frames[i] / 60.0f;

                char id[5]; sprintf(id, "##%02u", i);

                TableSetColumnIndex(x);

                if (Selectable(id, false, ImGuiSelectableFlags_AllowOverlap)) {
                    OpenPopup("Change register value");
                } SameLine(0.0, 0.0);

                if (BeginPopupContextItem(NULL, ImGuiPopupFlags_MouseButtonLeft)) {
                    static char new_value[9];

                    PushFont(lunar->font_small_code);
                    TextDisabled("Edit %s", mips_cc_r[i]);
                    PopFont();

                    PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6.0, 2.0));
                    PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0, 8.0));

                    PushFont(lunar->font_body);
                    PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.35, 0.35, 0.35, 0.35));

                    AlignTextToFramePadding();
                    Text(ICON_MS_EDIT); SameLine();

                    SetNextItemWidth(100);
                    PushFont(lunar->font_code);

                    if (InputText("##", new_value, 9, ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_EnterReturnsTrue)) {
                        if (new_value[0])
                            iop->r[i] = strtoul(new_value, NULL, 16);

                        CloseCurrentPopup();
                    }

                    PopFont();

                    if (Button("Change")) {
                        if (new_value[0])
                            iop->r[i] = strtoul(new_value, NULL, 16);

                        CloseCurrentPopup();
                    } SameLine();

                    if (Button("Cancel"))
                        CloseCurrentPopup();

                    PopStyleColor();
                    PopStyleVar(2);

                    PopFont();
                    EndPopup();
                }

                SameLine(0.0, 0.0);

                Text("%2s", mips_cc_r[i]); SameLine();
                TextColored(ImVec4(0.6 + a, 0.6, 0.6, 1.0), "%08x", iop->r[i]);

                ++i;
            }
        }

        EndTable();
    }

    PopFont();
}

static inline void show_work_in_progress(lunar::instance* lunar) {
    using namespace ImGui;

    Text("Work-in-progress!");
}

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

static int ee_sizing_combo = 3;
static int iop_sizing_combo = 0;

static const char* ee_reg_group_names[] = {
    "Main",
    "COP0",
    "FPU",
    "VU0",
    "VU1"
};

static int ee_reg_group = 0;

void show_ee_state(lunar::instance* lunar) {
    using namespace ImGui;

    if (Begin("EE state", &lunar->show_ee_state, ImGuiWindowFlags_MenuBar)) {
        if (BeginMenuBar()) {
            if (BeginMenu("Settings")) {
                if (BeginMenu(ICON_MS_CROP " Sizing")) {
                    for (int i = 0; i < 4; i++) {
                        if (Selectable(sizing_combo_items[i], i == ee_sizing_combo)) {
                            ee_table_sizing = table_sizing_flags[i];
                            ee_sizing_combo = i;
                        }
                    }

                    EndMenu();
                }

                EndMenu();
            }

            EndMenuBar();
        }

        if (BeginTable("table4", 5, ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_BordersInnerV)) {
            TableNextRow();

            for (int i = 0; i < 5; i++) {
                TableSetColumnIndex(i);

                if (Selectable(ee_reg_group_names[i], ee_reg_group == i)) {
                    ee_reg_group = i;
                }
            }

            EndTable();
        }

        if (BeginChild("ee#child")) {
            switch (ee_reg_group) {
                case 0: {
                    show_ee_main_registers(lunar);
                } break;

                case 1: {
                    show_ee_cop0_registers(lunar);
                } break;

                case 2: {
                    show_ee_fpu_registers(lunar);
                } break;

                case 3: {
                    show_work_in_progress(lunar);
                } break;

                case 4: {
                    show_work_in_progress(lunar);
                } break;
            }

            EndChild();
        }
    } End();

    for (int i = 0; i < 32; i++) {
        if (ee_fpu_frames[i]) 
            ee_fpu_frames[i]--;

        if (ee_cop0_frames[i])
            ee_cop0_frames[i]--;

        for (int j = 0; j < 4; j++)
            if (ee_frames[i].u32[j])
                ee_frames[i].u32[j]--;
    }
}

void show_iop_state(lunar::instance* lunar) {
    using namespace ImGui;

    if (Begin("IOP state", &lunar->show_ee_state, ImGuiWindowFlags_MenuBar)) {
        if (BeginMenuBar()) {
            if (BeginMenu("Settings")) {
                if (BeginMenu(ICON_MS_CROP " Sizing")) {
                    for (int i = 0; i < 4; i++) {
                        if (Selectable(sizing_combo_items[i], i == iop_sizing_combo)) {
                            iop_table_sizing = table_sizing_flags[i];
                            iop_sizing_combo = i;
                        }
                    }

                    EndMenu();
                }

                EndMenu();
            }

            EndMenuBar();
        }
        if (BeginChild("iop#child")) {
            show_iop_main_registers(lunar);

            EndChild();
        }
    } End();

    for (int i = 0; i < 32; i++)
        if (iop_frames[i])
            iop_frames[i]--;
}

}