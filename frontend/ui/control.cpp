#include <algorithm>
#include <vector>
#include <string>
#include <cctype>

#include "iris.hpp"

#include "res/IconsMaterialSymbols.h"

#include "ee/ee_dis.h"
#include "ee/ee_def.hpp"
#include "iop/iop_dis.h"

#define IM_RGB(r, g, b) ImVec4(((float)r / 255.0f), ((float)g / 255.0f), ((float)b / 255.0f), 1.0)

namespace iris {

struct ee_dis_state g_ee_dis_state;
struct iop_dis_state g_iop_dis_state;

void print_highlighted(const char* buf) {
    using namespace ImGui;

    std::vector <std::string> tokens;

    std::string text;

    while (*buf) {
        text.clear();        

        if (isalpha(*buf)) {
            while (isalpha(*buf) || isdigit(*buf) || (*buf == '.'))
                text.push_back(*buf++);
        } else if (isxdigit(*buf) || (*buf == '-')) {
            while (isxdigit(*buf) || (*buf == 'x') || (*buf == '-'))
                text.push_back(*buf++);
        } else if (*buf == '$') {
            while (*buf == '$' || isdigit(*buf) || isalpha(*buf) || *buf == '_')
                text.push_back(*buf++);
        } else if (*buf == ',') {
            while (*buf == ',')
                text.push_back(*buf++);
        } else if (*buf == '(') {
            while (*buf == '(')
                text.push_back(*buf++);
        } else if (*buf == ')') {
            while (*buf == ')')
                text.push_back(*buf++);
        } else if (*buf == '<') {
            while (*buf != '>')
                text.push_back(*buf++);

            text.push_back(*buf++);
        } else if (*buf == '_') {
            text.push_back(*buf++);
        } else if (*buf == '.') {
            text.push_back(*buf++);
        } else {
            printf("unhandled char %c (%d) \"%s\"\n", *buf, *buf, buf);

            exit(1);
        }

        while (isspace(*buf))
            text.push_back(*buf++);

        tokens.push_back(text);
    }

    for (const std::string& t : tokens) {
        if (isalpha(t[0])) {
            TextColored(IM_RGB(211, 167, 30), "%s", t.c_str());
        } else if (isdigit(t[0]) || t[0] == '-') {
            TextColored(IM_RGB(138, 143, 226), "%s", t.c_str());
        } else if (t[0] == '$') {
            TextColored(IM_RGB(68, 169, 240), "%s", t.c_str());
        } else if (t[0] == '<') {
            TextColored(IM_RGB(89, 89, 89), "%s", t.c_str());
        } else {
            Text("%s", t.c_str());
        }

        SameLine(0.0f, 0.0f);
    }

    NewLine();
}

static void show_ee_disassembly_view(iris::instance* iris) {
    using namespace ImGui;

    PushFont(iris->font_code);

    if (BeginTable("table1", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit)) {
        TableSetupColumn("a", ImGuiTableColumnFlags_NoResize, 15.0f);
        TableSetupColumn("b", ImGuiTableColumnFlags_NoResize, 15.0f);
        TableSetupColumn("c", ImGuiTableColumnFlags_NoResize);

        for (int row = -64; row < 64; row++) {
            if (iris->ee_control_follow_pc) {
                g_ee_dis_state.pc = iris->ps2->ee->pc + (row * 4);
            } else {
                g_ee_dis_state.pc = iris->ee_control_address + (row * 4);
            }

            TableNextRow();
            TableSetColumnIndex(0);

            PushFont(iris->font_icons);

            auto v = std::find_if(iris->breakpoints.begin(), iris->breakpoints.end(), [](breakpoint& a) {
                return a.addr == g_ee_dis_state.pc && a.cpu == BKPT_CPU_EE;
            });

            if (v != iris->breakpoints.end()) {
                Text(" " ICON_MS_FIBER_MANUAL_RECORD " ");
            }

            TableSetColumnIndex(1);

            if (g_ee_dis_state.pc == iris->ps2->ee->pc)
                Text(ICON_MS_CHEVRON_RIGHT " ");

            PopFont();

            TableSetColumnIndex(2);

            for (elf_symbol& sym : iris->symbols) {
                if (sym.addr == g_ee_dis_state.pc) {
                    PushFont(iris->font_icons);
                    TextColored(IM_RGB(211, 167, 30), ICON_MS_STAT_0); SameLine();
                    PopFont();
                    TextColored(IM_RGB(211, 167, 30), "%s", sym.name);

                    break;
                }
            }

            uint32_t opcode = ee_bus_read32(iris->ps2->ee_bus, g_ee_dis_state.pc & 0x1fffffff);

            char buf[128], id[16];

            char addr_str[9]; sprintf(addr_str, "%08x", g_ee_dis_state.pc);
            char opcode_str[9]; sprintf(opcode_str, "%08x", opcode);
            char* disassembly = ee_disassemble(buf, opcode, &g_ee_dis_state);

            sprintf(id, "##%d", row);

            Selectable(id, false, ImGuiSelectableFlags_AllowDoubleClick | ImGuiSelectableFlags_SpanAllColumns);

            if (IsItemHovered() && IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                breakpoint b;

                b.addr = g_ee_dis_state.pc;
                b.cond_r = false;
                b.cond_w = false;
                b.cond_x = true;
                b.cpu = BKPT_CPU_EE;
                b.size = 4;
                b.enabled = true;

                auto addr = std::find_if(iris->breakpoints.begin(), iris->breakpoints.end(), [](breakpoint& a) {
                    return a.addr == g_ee_dis_state.pc && a.cpu == BKPT_CPU_EE;
                });

                if (addr == iris->breakpoints.end()) {
                    iris->breakpoints.push_back(b);
                } else {
                    iris->breakpoints.erase(addr);
                }
            } SameLine();

            if (BeginPopupContextItem()) {
                PushFont(iris->font_small_code);
                TextDisabled("0x%08x", g_ee_dis_state.pc);
                PopFont();

                PushFont(iris->font_body);

                if (BeginMenu(ICON_MS_CONTENT_COPY "  Copy")) {
                    if (Selectable(ICON_MS_SORT "  Address")) {
                        SDL_SetClipboardText(addr_str);
                    }

                    if (Selectable(ICON_MS_SORT "  Opcode")) {
                        SDL_SetClipboardText(opcode_str);
                    }

                    if (Selectable(ICON_MS_SORT "  Disassembly")) {
                        SDL_SetClipboardText(disassembly);
                    }

                    ImGui::EndMenu();
                }

                auto addr = std::find_if(iris->breakpoints.begin(), iris->breakpoints.end(), [](breakpoint& a) {
                    return a.addr == g_ee_dis_state.pc && a.cpu == BKPT_CPU_EE;
                });

                if (addr != iris->breakpoints.end()) {
                    if (MenuItem(ICON_MS_CANCEL "  Remove this breakpoint")) {
                        iris->breakpoints.erase(addr);
                    }
                } else {
                    if (MenuItem(ICON_MS_ADD_CIRCLE "  Add breakpoint here")) {
                        breakpoint b;

                        b.addr = g_ee_dis_state.pc;
                        b.cond_r = false;
                        b.cond_w = false;
                        b.cond_x = true;
                        b.cpu = BKPT_CPU_EE;
                        b.size = 4;
                        b.enabled = true;

                        iris->breakpoints.push_back(b);
                    }
                }

                PopFont();
                EndPopup();
            }

            Text("%s ", addr_str); SameLine();
            TextDisabled("%s ", opcode_str); SameLine();

            if (true) {
                print_highlighted(disassembly);
            } else {
                Text("%s", disassembly);
            }

            if (iris->ee_control_follow_pc) {
                if (g_ee_dis_state.pc == iris->ps2->ee->pc)
                    SetScrollHereY(0.5f);
            } else {
                if (g_ee_dis_state.pc == iris->ee_control_address)
                    SetScrollHereY(0.5f);
            }
        }

        EndTable();
    }

    PopFont();
}

static void show_iop_disassembly_view(iris::instance* iris) {
    using namespace ImGui;

    PushFont(iris->font_code);

    if (BeginTable("table2", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit)) {
        TableSetupColumn("a", ImGuiTableColumnFlags_NoResize, 15.0f);
        TableSetupColumn("b", ImGuiTableColumnFlags_NoResize, 15.0f);
        TableSetupColumn("c", ImGuiTableColumnFlags_NoResize);

        for (int row = -64; row < 64; row++) {
            if (iris->iop_control_follow_pc) {
                g_iop_dis_state.addr = iris->ps2->iop->pc + (row * 4);
            } else {
                g_iop_dis_state.addr = iris->iop_control_address + (row * 4);
            }

            TableNextRow();
            TableSetColumnIndex(0);

            PushFont(iris->font_icons);

            auto v = std::find_if(iris->breakpoints.begin(), iris->breakpoints.end(), [](breakpoint& a) {
                return a.addr == g_iop_dis_state.addr && a.cpu == BKPT_CPU_IOP;
            });

            if (v != iris->breakpoints.end()) {
                Text(" " ICON_MS_FIBER_MANUAL_RECORD " ");
            }

            TableSetColumnIndex(1);

            if (g_iop_dis_state.addr == iris->ps2->iop->pc)
                Text(ICON_MS_CHEVRON_RIGHT " ");

            PopFont();

            TableSetColumnIndex(2);

            uint32_t opcode = iop_bus_read32(iris->ps2->iop_bus, g_iop_dis_state.addr & 0x1fffffff);

            char buf[128];

            char addr_str[9]; sprintf(addr_str, "%08x", g_iop_dis_state.addr);
            char opcode_str[9]; sprintf(opcode_str, "%08x", opcode);
            char* disassembly = iop_disassemble(buf, opcode, &g_iop_dis_state);

            Text("%s ", addr_str); SameLine();
            TextDisabled("%s ", opcode_str); SameLine();

            char id[16];

            sprintf(id, "##%d", row);

            Selectable(id, false, ImGuiSelectableFlags_AllowDoubleClick | ImGuiSelectableFlags_SpanAllColumns);

            if (IsItemHovered() && IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                breakpoint b;

                b.addr = g_iop_dis_state.addr;
                b.cond_r = false;
                b.cond_w = false;
                b.cond_x = true;
                b.cpu = BKPT_CPU_IOP;
                b.size = 4;
                b.enabled = true;

                auto addr = std::find_if(iris->breakpoints.begin(), iris->breakpoints.end(), [](breakpoint& a) {
                    return a.addr == g_iop_dis_state.addr && a.cpu == BKPT_CPU_IOP;
                });

                if (addr == iris->breakpoints.end()) {
                    iris->breakpoints.push_back(b);
                } else {
                    iris->breakpoints.erase(addr);
                }
            } SameLine();

            if (BeginPopupContextItem()) {
                PushFont(iris->font_small_code);
                TextDisabled("0x%08x", g_iop_dis_state.addr);
                PopFont();

                PushFont(iris->font_body);

                if (BeginMenu(ICON_MS_CONTENT_COPY "  Copy")) {
                    if (Selectable(ICON_MS_SORT "  Address")) {
                        SDL_SetClipboardText(addr_str);
                    }

                    if (Selectable(ICON_MS_SORT "  Opcode")) {
                        SDL_SetClipboardText(opcode_str);
                    }

                    if (Selectable(ICON_MS_SORT "  Disassembly")) {
                        SDL_SetClipboardText(disassembly);
                    }

                    ImGui::EndMenu();
                }

                auto addr = std::find_if(iris->breakpoints.begin(), iris->breakpoints.end(), [](breakpoint& a) {
                    return a.addr == g_iop_dis_state.addr && a.cpu == BKPT_CPU_IOP;
                });

                if (addr != iris->breakpoints.end()) {
                    if (MenuItem(ICON_MS_CANCEL "  Remove this breakpoint")) {
                        iris->breakpoints.erase(addr);
                    }
                } else {
                    if (MenuItem(ICON_MS_ADD_CIRCLE "  Add breakpoint here")) {
                        breakpoint b;

                        b.addr = g_iop_dis_state.addr;
                        b.cond_r = false;
                        b.cond_w = false;
                        b.cond_x = true;
                        b.cpu = BKPT_CPU_IOP;
                        b.size = 4;
                        b.enabled = true;

                        iris->breakpoints.push_back(b);
                    }
                }

                PopFont();
                EndPopup();
            }

            if (true) {
                print_highlighted(disassembly);
            } else {
                Text("%s", disassembly);
            }

            if (iris->iop_control_follow_pc) {
                if (g_iop_dis_state.addr == iris->ps2->iop->pc)
                    SetScrollHereY(0.5f);
            } else {
                if (g_iop_dis_state.addr == iris->iop_control_address)
                    SetScrollHereY(0.5f);
            }
        } EndTable();
    }

    PopFont();
}

void show_ee_control(iris::instance* iris) {
    using namespace ImGui;

    PushFont(iris->font_icons);

    if (Begin("EE (R5900)", &iris->show_ee_control)) {
        if (Button(iris->pause ? ICON_MS_PLAY_ARROW : ICON_MS_PAUSE, ImVec2(50, 0))) {
            iris->pause = !iris->pause;
        } SameLine();

        if (Button(ICON_MS_STEP_INTO)) {
            iris->pause = true;
            iris->step = true;
        } SameLine();

        if (Button(ICON_MS_STEP_OVER)) {
            iris->step_over = true;
            iris->step_over_addr = iris->ps2->ee->pc + 4;
            iris->pause = false;
        } SameLine();

        if (Button(ICON_MS_STEP_OUT)) {
            iris->step_out = true;
            iris->pause = false;
        } SameLine();

        if (Button(ICON_MS_MOVE_DOWN)) {
            iris->ee_control_follow_pc = true;
        }

        if (iris->symbols.size()) {
            TextDisabled("Current function:"); SameLine();

            const char* func = "<unknown>";

            for (elf_symbol& sym : iris->symbols) {
                if (iris->ps2->ee->pc >= sym.addr && iris->ps2->ee->pc < (sym.addr + sym.size)) {
                    func = sym.name;

                    break;
                }
            }

            Text("%s", func);
        }

        SeparatorText("Disassembly");

        if (BeginChild("ee##disassembly")) {
            show_ee_disassembly_view(iris);
        } EndChild();
    } End();

    PopFont();
}

void show_iop_control(iris::instance* iris) {
    using namespace ImGui;

    PushFont(iris->font_icons);

    if (Begin("IOP (R3000)", &iris->show_iop_control)) {
        if (Button(iris->pause ? ICON_MS_PLAY_ARROW : ICON_MS_PAUSE, ImVec2(50, 0))) {
            iris->pause = !iris->pause;
        } SameLine();

        if (Button(ICON_MS_STEP)) {
            iris->pause = true;

            ps2_iop_cycle(iris->ps2);
        }

        SeparatorText("Disassembly");

        if (BeginChild("iop##disassembly")) {
            show_iop_disassembly_view(iris);
        } EndChild();
    } End();

    PopFont();
}

}