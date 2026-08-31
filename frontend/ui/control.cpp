#include <algorithm>
#include <vector>
#include <string>
#include <cctype>

#include "iris.hpp"

#include "res/IconsMaterialSymbols.h"
#include "imgui_internal.h"

#include "ee/ee_dis.hpp"
#include "ee/ee_def.hpp"
#include "ee/vu_def.hpp"
#include "iop/iop_def.hpp"
#include "iop/iop_dis.hpp"
#include "ps2.hpp"

#define IM_RGB(r, g, b) ImVec4(((float)r / 255.0f), ((float)g / 255.0f), ((float)b / 255.0f), 1.0)

namespace iris {

static ee::dis::Dis g_ee_dis_state;
static iop::dis::Dis g_iop_dis_state;

void print_highlighted(Instance* iris, const char* buf) {
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
            iris_warning(&iris->log.ui, "Unhandled char {} ({}) \"{}\"", *buf, (int)*buf, buf);

            exit(1);
        }

        while (isspace(*buf))
            text.push_back(*buf++);

        tokens.push_back(text);
    }

    for (const std::string& t : tokens) {
        if (isalpha(t[0])) {
            ImVec4 col = ImVec4(
                iris->ui.codeview_color_mnemonic.Value.x,
                iris->ui.codeview_color_mnemonic.Value.y,
                iris->ui.codeview_color_mnemonic.Value.z,
                iris->ui.codeview_color_mnemonic.Value.w
            );

            TextColored(col, "%s", t.c_str());
        } else if (isdigit(t[0]) || t[0] == '-') {
            ImVec4 col = ImVec4(
                iris->ui.codeview_color_number.Value.x,
                iris->ui.codeview_color_number.Value.y,
                iris->ui.codeview_color_number.Value.z,
                iris->ui.codeview_color_number.Value.w
            );

            TextColored(col, "%s", t.c_str());
        } else if (t[0] == '$') {
            ImVec4 col = ImVec4(
                iris->ui.codeview_color_register.Value.x,
                iris->ui.codeview_color_register.Value.y,
                iris->ui.codeview_color_register.Value.z,
                iris->ui.codeview_color_register.Value.w
            );

            TextColored(col, "%s", t.c_str());
        } else if (t[0] == '<') {
            ImVec4 col = ImVec4(
                iris->ui.codeview_color_other.Value.x,
                iris->ui.codeview_color_other.Value.y,
                iris->ui.codeview_color_other.Value.z,
                iris->ui.codeview_color_other.Value.w
            );

            TextColored(col, "%s", t.c_str());
        } else {
            Text("%s", t.c_str());
        }

        SameLine(0.0f, 0.0f);
    }

    NewLine();
}

void show_ee_disassembly_view(Instance* iris) {
    using namespace ImGui;

    float font_scale = GetStyle().FontScaleMain;

    GetStyle().FontScaleMain = iris->ui.codeview_font_scale;

    PushFont(iris->ui.font_code);

    if (!iris->ui.codeview_use_theme_background) {
        PushStyleColor(ImGuiCol_TableRowBg, ImVec4(
            iris->ui.codeview_color_background.Value.x,
            iris->ui.codeview_color_background.Value.y,
            iris->ui.codeview_color_background.Value.z,
            iris->ui.codeview_color_background.Value.w
        ));
        PushStyleColor(ImGuiCol_TableRowBgAlt, ImVec4(
            iris->ui.codeview_color_background.Value.x,
            iris->ui.codeview_color_background.Value.y,
            iris->ui.codeview_color_background.Value.z,
            iris->ui.codeview_color_background.Value.w
        ));
        PushStyleColor(ImGuiCol_Text, ImVec4(
            iris->ui.codeview_color_text.Value.x,
            iris->ui.codeview_color_text.Value.y,
            iris->ui.codeview_color_text.Value.z,
            iris->ui.codeview_color_text.Value.w
        ));
        PushStyleColor(ImGuiCol_TextDisabled, ImVec4(
            iris->ui.codeview_color_comment.Value.x,
            iris->ui.codeview_color_comment.Value.y,
            iris->ui.codeview_color_comment.Value.z,
            iris->ui.codeview_color_comment.Value.w
        ));
    }

    if (BeginTable("table1", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit)) {
        TableSetupColumn("a", ImGuiTableColumnFlags_NoResize, 15.0f);
        TableSetupColumn("b", ImGuiTableColumnFlags_NoResize, 15.0f);
        TableSetupColumn("c", ImGuiTableColumnFlags_NoResize);

        for (int row = -64; row < 64; row++) {
            if (iris->debug.ee_control_follow_pc) {
                g_ee_dis_state.pc = iris->ps2->ee->pc + (row * 4);
            } else {
                g_ee_dis_state.pc = iris->debug.ee_control_address + (row * 4);
            }

            TableNextRow();
            TableSetColumnIndex(0);

            PushFont(iris->ui.font_icons);

            auto v = std::find_if(iris->debug.breakpoints.begin(), iris->debug.breakpoints.end(), [](Breakpoint& a) {
                return a.addr == g_ee_dis_state.pc && a.cpu == BreakpointCpu::EE;
            });

            if (v != iris->debug.breakpoints.end()) {
                Text(" " ICON_MS_FIBER_MANUAL_RECORD " ");
            }

            TableSetColumnIndex(1);

            if (g_ee_dis_state.pc == iris->ps2->ee->pc)
                Text(ICON_MS_CHEVRON_RIGHT " ");

            PopFont();

            TableSetColumnIndex(2);

            for (elf::Symbol& sym : iris->debug.symbols) {
                if (sym.addr == g_ee_dis_state.pc) {
                    ImVec4 col = ImVec4(
                        iris->ui.codeview_color_mnemonic.Value.x,
                        iris->ui.codeview_color_mnemonic.Value.y,
                        iris->ui.codeview_color_mnemonic.Value.z,
                        iris->ui.codeview_color_mnemonic.Value.w
                    );

                    PushFont(iris->ui.font_icons);
                    TextColored(col, ICON_MS_STAT_0); SameLine();
                    PopFont();
                    TextColored(col, "%s", sym.name);

                    break;
                }
            }

            uint32_t opcode = ee::bus::read32(iris->ps2->ee_bus, g_ee_dis_state.pc & 0x1fffffff);

            char buf[128], id[16];

            char addr_str[9]; sprintf(addr_str, "%08x", g_ee_dis_state.pc);
            char opcode_str[9]; sprintf(opcode_str, "%08x", opcode);
            char* disassembly = ee::dis::disassemble(buf, opcode, &g_ee_dis_state);

            sprintf(id, "##%d", row);

            Selectable(id, false, ImGuiSelectableFlags_AllowDoubleClick | ImGuiSelectableFlags_SpanAllColumns);

            if (IsItemHovered() && IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                Breakpoint b;

                b.addr = g_ee_dis_state.pc;
                b.cond_r = false;
                b.cond_w = false;
                b.cond_x = true;
                b.cpu = BreakpointCpu::EE;
                b.size = 4;
                b.enabled = true;

                auto addr = std::find_if(iris->debug.breakpoints.begin(), iris->debug.breakpoints.end(), [](Breakpoint& a) {
                    return a.addr == g_ee_dis_state.pc && a.cpu == BreakpointCpu::EE;
                });

                if (addr == iris->debug.breakpoints.end()) {
                    iris->debug.breakpoints.push_back(b);
                } else {
                    iris->debug.breakpoints.erase(addr);
                }
            } SameLine();

            if (BeginPopupContextItem()) {
                PushFont(iris->ui.font_small_code);
                TextDisabled("0x%08x", g_ee_dis_state.pc);
                PopFont();

                PushFont(iris->ui.font_body);

                if (imgui::BeginMenu(ICON_MS_CONTENT_COPY "  Copy")) {
                    if (imgui::Selectable(ICON_MS_SORT "  Address")) {
                        SDL_SetClipboardText(addr_str);
                    }

                    if (imgui::Selectable(ICON_MS_SORT "  Opcode")) {
                        SDL_SetClipboardText(opcode_str);
                    }

                    if (imgui::Selectable(ICON_MS_SORT "  Disassembly")) {
                        SDL_SetClipboardText(disassembly);
                    }

                    ImGui::EndMenu();
                }

                auto addr = std::find_if(iris->debug.breakpoints.begin(), iris->debug.breakpoints.end(), [](Breakpoint& a) {
                    return a.addr == g_ee_dis_state.pc && a.cpu == BreakpointCpu::EE;
                });

                if (addr != iris->debug.breakpoints.end()) {
                    if (imgui::MenuItem(ICON_MS_CANCEL "  Remove this breakpoint")) {
                        iris->debug.breakpoints.erase(addr);
                    }
                } else {
                    if (imgui::MenuItem(ICON_MS_ADD_CIRCLE "  Add breakpoint here")) {
                        Breakpoint b;

                        b.addr = g_ee_dis_state.pc;
                        b.cond_r = false;
                        b.cond_w = false;
                        b.cond_x = true;
                        b.cpu = BreakpointCpu::EE;
                        b.size = 4;
                        b.enabled = true;

                        iris->debug.breakpoints.push_back(b);
                    }
                }

                Separator();

                if (imgui::MenuItem(ICON_MS_CODE_OFF "  Patch with NOP")) {
                    ee::bus::write32(iris->ps2->ee_bus, g_ee_dis_state.pc & 0x1fffffff, 0);

                    ee::flush_cache(iris->ps2->ee);
                }

                PopFont();
                EndPopup();
            }

            Text("%s ", addr_str); SameLine();
            TextDisabled("%s ", opcode_str); SameLine();

            if (true) {
                print_highlighted(iris, disassembly);
            } else {
                Text("%s", disassembly);
            }

            if (iris->debug.ee_control_follow_pc) {
                if (g_ee_dis_state.pc == iris->ps2->ee->pc)
                    SetScrollHereY(0.5f);
            } else {
                if (g_ee_dis_state.pc == iris->debug.ee_control_address)
                    SetScrollHereY(0.5f);
            }
        }

        EndTable();
    }

    if (!iris->ui.codeview_use_theme_background) {
        PopStyleColor(4);
    }

    PopFont();

    GetStyle().FontScaleMain = font_scale;
}

void show_iop_disassembly_view(Instance* iris) {
    using namespace ImGui;

    float font_scale = GetStyle().FontScaleMain;

    GetStyle().FontScaleMain = iris->ui.codeview_font_scale;

    PushFont(iris->ui.font_code);

    if (!iris->ui.codeview_use_theme_background) {
        PushStyleColor(ImGuiCol_TableRowBg, ImVec4(
            iris->ui.codeview_color_background.Value.x,
            iris->ui.codeview_color_background.Value.y,
            iris->ui.codeview_color_background.Value.z,
            iris->ui.codeview_color_background.Value.w
        ));
        PushStyleColor(ImGuiCol_TableRowBgAlt, ImVec4(
            iris->ui.codeview_color_background.Value.x,
            iris->ui.codeview_color_background.Value.y,
            iris->ui.codeview_color_background.Value.z,
            iris->ui.codeview_color_background.Value.w
        ));
        PushStyleColor(ImGuiCol_Text, ImVec4(
            iris->ui.codeview_color_text.Value.x,
            iris->ui.codeview_color_text.Value.y,
            iris->ui.codeview_color_text.Value.z,
            iris->ui.codeview_color_text.Value.w
        ));
        PushStyleColor(ImGuiCol_TextDisabled, ImVec4(
            iris->ui.codeview_color_comment.Value.x,
            iris->ui.codeview_color_comment.Value.y,
            iris->ui.codeview_color_comment.Value.z,
            iris->ui.codeview_color_comment.Value.w
        ));
    }

    if (BeginTable("table2", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit)) {
        TableSetupColumn("a", ImGuiTableColumnFlags_NoResize, 15.0f);
        TableSetupColumn("b", ImGuiTableColumnFlags_NoResize, 15.0f);
        TableSetupColumn("c", ImGuiTableColumnFlags_NoResize);

        for (int row = -64; row < 64; row++) {
            if (iris->debug.iop_control_follow_pc) {
                g_iop_dis_state.addr = iris->ps2->iop->pc + (row * 4);
            } else {
                g_iop_dis_state.addr = iris->debug.iop_control_address + (row * 4);
            }

            TableNextRow();
            TableSetColumnIndex(0);

            PushFont(iris->ui.font_icons);

            auto v = std::find_if(iris->debug.breakpoints.begin(), iris->debug.breakpoints.end(), [](Breakpoint& a) {
                return a.addr == g_iop_dis_state.addr && a.cpu == BreakpointCpu::IOP;
            });

            if (v != iris->debug.breakpoints.end()) {
                Text(" " ICON_MS_FIBER_MANUAL_RECORD " ");
            }

            TableSetColumnIndex(1);

            if (g_iop_dis_state.addr == iris->ps2->iop->pc)
                Text(ICON_MS_CHEVRON_RIGHT " ");

            PopFont();

            TableSetColumnIndex(2);

            uint32_t opcode = iop::bus::read32(iris->ps2->iop_bus, g_iop_dis_state.addr & 0x1fffffff);

            char buf[128];

            char addr_str[9]; sprintf(addr_str, "%08x", g_iop_dis_state.addr);
            char opcode_str[9]; sprintf(opcode_str, "%08x", opcode);
            char* disassembly = iop::dis::disassemble(buf, opcode, &g_iop_dis_state);

            Text("%s ", addr_str); SameLine();
            TextDisabled("%s ", opcode_str); SameLine();

            char id[16];

            sprintf(id, "##%d", row);

            Selectable(id, false, ImGuiSelectableFlags_AllowDoubleClick | ImGuiSelectableFlags_SpanAllColumns);

            if (IsItemHovered() && IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                Breakpoint b;

                b.addr = g_iop_dis_state.addr;
                b.cond_r = false;
                b.cond_w = false;
                b.cond_x = true;
                b.cpu = BreakpointCpu::IOP;
                b.size = 4;
                b.enabled = true;

                auto addr = std::find_if(iris->debug.breakpoints.begin(), iris->debug.breakpoints.end(), [](Breakpoint& a) {
                    return a.addr == g_iop_dis_state.addr && a.cpu == BreakpointCpu::IOP;
                });

                if (addr == iris->debug.breakpoints.end()) {
                    iris->debug.breakpoints.push_back(b);
                } else {
                    iris->debug.breakpoints.erase(addr);
                }
            } SameLine();

            if (BeginPopupContextItem()) {
                PushFont(iris->ui.font_small_code);
                TextDisabled("0x%08x", g_iop_dis_state.addr);
                PopFont();

                PushFont(iris->ui.font_body);

                if (imgui::BeginMenu(ICON_MS_CONTENT_COPY "  Copy")) {
                    if (imgui::Selectable(ICON_MS_SORT "  Address")) {
                        SDL_SetClipboardText(addr_str);
                    }

                    if (imgui::Selectable(ICON_MS_SORT "  Opcode")) {
                        SDL_SetClipboardText(opcode_str);
                    }

                    if (imgui::Selectable(ICON_MS_SORT "  Disassembly")) {
                        SDL_SetClipboardText(disassembly);
                    }

                    ImGui::EndMenu();
                }

                auto addr = std::find_if(iris->debug.breakpoints.begin(), iris->debug.breakpoints.end(), [](Breakpoint& a) {
                    return a.addr == g_iop_dis_state.addr && a.cpu == BreakpointCpu::IOP;
                });

                if (addr != iris->debug.breakpoints.end()) {
                    if (imgui::MenuItem(ICON_MS_CANCEL "  Remove this breakpoint")) {
                        iris->debug.breakpoints.erase(addr);
                    }
                } else {
                    if (imgui::MenuItem(ICON_MS_ADD_CIRCLE "  Add breakpoint here")) {
                        Breakpoint b;

                        b.addr = g_iop_dis_state.addr;
                        b.cond_r = false;
                        b.cond_w = false;
                        b.cond_x = true;
                        b.cpu = BreakpointCpu::IOP;
                        b.size = 4;
                        b.enabled = true;

                        iris->debug.breakpoints.push_back(b);
                    }
                }

                Separator();

                if (imgui::MenuItem(ICON_MS_CODE_OFF "  Patch with NOP")) {
                    iop::bus::write32(iris->ps2->iop_bus, g_iop_dis_state.addr & 0x1fffffff, 0);

                    iop::flush_cache(iris->ps2->iop);
                }

                PopFont();
                EndPopup();
            }

            if (true) {
                print_highlighted(iris, disassembly);
            } else {
                Text("%s", disassembly);
            }

            if (iris->debug.iop_control_follow_pc) {
                if (g_iop_dis_state.addr == iris->ps2->iop->pc)
                    SetScrollHereY(0.5f);
            } else {
                if (g_iop_dis_state.addr == iris->debug.iop_control_address)
                    SetScrollHereY(0.5f);
            }
        } EndTable();
    }

    PopFont();

    if (!iris->ui.codeview_use_theme_background) {
        PopStyleColor(4);
    }

    GetStyle().FontScaleMain = font_scale;
}

bool EeControl::begin() {
    ImGui::PushFont(iris->ui.font_icons);

    return Applet::begin();
}

void EeControl::end() {
    ImGui::End();
    ImGui::PopFont();
}

void EeControl::on_render() {
    using namespace ImGui;

    if (Button(iris->debug.pause ? ICON_MS_PLAY_ARROW : ICON_MS_PAUSE, ImVec2(50, 0))) {
        iris->debug.pause = !iris->debug.pause;
    } SameLine();

    if (Button(ICON_MS_STEP_INTO)) {
        iris->debug.pause = true;
        iris->debug.step = true;
    } SameLine();

    if (Button(ICON_MS_STEP_OVER)) {
        iris->debug.step_over = true;
        iris->debug.step_over_addr = iris->ps2->ee->pc + 4;
        iris->debug.pause = false;
    } SameLine();

    if (Button(ICON_MS_STEP_OUT)) {
        iris->debug.step_out = true;
        iris->debug.pause = false;
    } SameLine();

    if (Button(ICON_MS_MOVE_DOWN)) {
        iris->debug.ee_control_follow_pc = true;
    } SameLine();

    if (Button(ICON_MS_AUTORENEW)) {
        ee::flush_cache(iris->ps2->ee);
    } SameLine();

    SeparatorEx(ImGuiSeparatorFlags_Vertical); SameLine();
    AlignTextToFramePadding();
    Text("Go to"); SameLine();
    PushFont(iris->ui.font_code);
    SetNextItemWidth(CalcTextSize("00000000   ").x);
    InputInt("##address", (int32_t*)&iris->debug.ee_control_address, 0, 0, ImGuiInputTextFlags_CharsHexadecimal);
    PopFont();

    if (IsItemDeactivatedAfterEdit()) {
        iris->debug.ee_control_follow_pc = false;
    }

    if (iris->debug.symbols.size()) {
        TextDisabled("Current function:"); SameLine();

        const char* func = "<unknown>";

        for (elf::Symbol& sym : iris->debug.symbols) {
            if (iris->ps2->ee->pc >= sym.addr && iris->ps2->ee->pc < (sym.addr + sym.size)) {
                func = sym.name;

                break;
            }
        }

        Text("%s", func);
    }

    imgui::section(iris, "Disassembly");

    if (BeginChild("ee##disassembly")) {
        show_ee_disassembly_view(iris);
    } EndChild();
}

bool IopControl::begin() {
    ImGui::PushFont(iris->ui.font_icons);

    return Applet::begin();
}

void IopControl::end() {
    ImGui::End();
    ImGui::PopFont();
}

void IopControl::on_render() {
    using namespace ImGui;

    if (Button(iris->debug.pause ? ICON_MS_PLAY_ARROW : ICON_MS_PAUSE, ImVec2(50, 0))) {
        iris->debug.pause = !iris->debug.pause;
    } SameLine();

    if (Button(ICON_MS_STEP)) {
        iris->debug.pause = true;

        ps2::step_iop(iris->ps2);
    } SameLine();

    if (Button(ICON_MS_MOVE_DOWN)) {
        iris->debug.iop_control_follow_pc = true;
    } SameLine();

    SeparatorEx(ImGuiSeparatorFlags_Vertical); SameLine();
    AlignTextToFramePadding();
    Text("Go to"); SameLine();
    PushFont(iris->ui.font_code);
    SetNextItemWidth(CalcTextSize("00000000   ").x);
    InputInt("##address", (int32_t*)&iris->debug.iop_control_address, 0, 0, ImGuiInputTextFlags_CharsHexadecimal);
    PopFont();

    if (IsItemDeactivatedAfterEdit()) {
        iris->debug.iop_control_follow_pc = false;
    }

    imgui::section(iris, "Disassembly");

    if (BeginChild("iop##disassembly")) {
        show_iop_disassembly_view(iris);
    } EndChild();
}

}