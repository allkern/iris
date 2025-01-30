#include <vector>
#include <string>
#include <cctype>

#include "instance.hpp"

#include "res/IconsMaterialSymbols.h"

#include "ee/ee_dis.h"
#include "iop/iop_dis.h"

#define IM_RGB(r, g, b) ImVec4(((float)r / 255.0f), ((float)g / 255.0f), ((float)b / 255.0f), 1.0)

namespace lunar {

struct ee_dis_state g_ee_dis_state;
struct iop_dis_state g_iop_dis_state;

void print_highlighted(const char* buf) {
    using namespace ImGui;

    std::vector <std::string> tokens;

    std::string text;

    while (*buf) {
        text.clear();        

        if (isalpha(*buf)) {
            while (isalpha(*buf) || isdigit(*buf))
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
            TextColored(IM_RGB(211, 167, 30), t.c_str());
        } else if (isdigit(t[0]) || t[0] == '-') {
            TextColored(IM_RGB(138, 143, 226), t.c_str());
        } else if (t[0] == '$') {
            TextColored(IM_RGB(68, 169, 240), t.c_str());
        } else if (t[0] == '<') {
            TextColored(IM_RGB(89, 89, 89), t.c_str());
        } else {
            Text(t.c_str());
        }

        SameLine(0.0f, 0.0f);
    }

    NewLine();
}

static void show_ee_disassembly_view(lunar::instance* lunar) {
    using namespace ImGui;

    PushFont(lunar->font_code);

    if (BeginTable("table1", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit)) {
        TableSetupColumn("a", ImGuiTableColumnFlags_NoResize, 15.0f);
        TableSetupColumn("b", ImGuiTableColumnFlags_NoResize, 15.0f);
        TableSetupColumn("c", ImGuiTableColumnFlags_NoResize);

        for (int row = -64; row < 64; row++) {
            g_ee_dis_state.pc = lunar->ps2->ee->pc + (row * 4);

            TableNextRow();
            TableSetColumnIndex(0);

            PushFont(lunar->font_icons);

            TableSetColumnIndex(1);

            if (g_ee_dis_state.pc == lunar->ps2->ee->pc)
                Text(ICON_MS_CHEVRON_RIGHT " ");

            PopFont();

            TableSetColumnIndex(2);

            uint32_t opcode = ee_bus_read32(lunar->ps2->ee_bus, g_ee_dis_state.pc & 0x1fffffff);

            char buf[128];

            char addr_str[9]; sprintf(addr_str, "%08x", g_ee_dis_state.pc);
            char opcode_str[9]; sprintf(opcode_str, "%08x", opcode);
            char* disassembly = ee_disassemble(buf, opcode, &g_ee_dis_state);

            Text("%s ", addr_str); SameLine();
            TextDisabled("%s ", opcode_str); SameLine();

            char id[16];

            sprintf(id, "##%d", row);

            if (Selectable(id, false, ImGuiSelectableFlags_AllowDoubleClick | ImGuiSelectableFlags_SpanAllColumns)) {
            } SameLine();

            if (BeginPopupContextItem()) {
                PushFont(lunar->font_small_code);
                TextDisabled("0x%08x", g_ee_dis_state.pc);
                PopFont();

                PushFont(lunar->font_body);

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

                PopFont();
                EndPopup();
            }

            if (true) {
                print_highlighted(disassembly);
            } else {
                Text(disassembly);
            }

            if (g_ee_dis_state.pc == lunar->ps2->ee->pc)
                SetScrollHereY(0.5f);
        }

        EndTable();
    }

    PopFont();
}

static void show_iop_disassembly_view(lunar::instance* lunar) {
    using namespace ImGui;

    PushFont(lunar->font_code);

    if (BeginTable("table2", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit)) {
        TableSetupColumn("a", ImGuiTableColumnFlags_NoResize, 15.0f);
        TableSetupColumn("b", ImGuiTableColumnFlags_NoResize, 15.0f);
        TableSetupColumn("c", ImGuiTableColumnFlags_NoResize);

        for (int row = -64; row < 64; row++) {
            g_iop_dis_state.addr = lunar->ps2->iop->pc + (row * 4);

            TableNextRow();
            TableSetColumnIndex(0);

            PushFont(lunar->font_icons);

            TableSetColumnIndex(1);

            if (g_iop_dis_state.addr == lunar->ps2->iop->pc)
                Text(ICON_MS_CHEVRON_RIGHT " ");

            PopFont();

            TableSetColumnIndex(2);

            uint32_t opcode = iop_bus_read32(lunar->ps2->iop_bus, g_iop_dis_state.addr & 0x1fffffff);

            char buf[128];

            char addr_str[9]; sprintf(addr_str, "%08x", g_iop_dis_state.addr);
            char opcode_str[9]; sprintf(opcode_str, "%08x", opcode);
            char* disassembly = iop_disassemble(buf, opcode, &g_iop_dis_state);

            Text("%s ", addr_str); SameLine();
            TextDisabled("%s ", opcode_str); SameLine();

            char id[16];

            sprintf(id, "##%d", row);

            if (Selectable(id, false, ImGuiSelectableFlags_AllowDoubleClick | ImGuiSelectableFlags_SpanAllColumns)) {
            } SameLine();

            if (BeginPopupContextItem()) {
                PushFont(lunar->font_small_code);
                TextDisabled("0x%08x", g_iop_dis_state.addr);
                PopFont();

                PushFont(lunar->font_body);

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

                PopFont();
                EndPopup();
            }

            if (true) {
                print_highlighted(disassembly);
            } else {
                Text(disassembly);
            }

            if (g_iop_dis_state.addr == lunar->ps2->iop->pc)
                SetScrollHereY(0.5f);
        }

        EndTable();
    }

    PopFont();
}

void show_ee_control(lunar::instance* lunar) {
    using namespace ImGui;

    PushFont(lunar->font_icons);

    if (Begin("EE (R5900)", &lunar->show_ee_control)) {
        if (Button(lunar->pause ? ICON_MS_PLAY_ARROW : ICON_MS_PAUSE, ImVec2(50, 0))) {
            lunar->pause = !lunar->pause;
        } SameLine();

        if (Button(ICON_MS_STEP)) {
            lunar->pause = true;
            lunar->step = true;
        }

        SeparatorText("Disassembly");

        if (BeginChild("ee##disassembly")) {
            show_ee_disassembly_view(lunar);
        } EndChild();

        End();
    }

    PopFont();
}

void show_iop_control(lunar::instance* lunar) {
    using namespace ImGui;

    PushFont(lunar->font_icons);

    if (Begin("IOP (R3000)", &lunar->show_iop_control)) {
        if (Button(lunar->pause ? ICON_MS_PLAY_ARROW : ICON_MS_PAUSE, ImVec2(50, 0))) {
            lunar->pause = !lunar->pause;
        } SameLine();

        if (Button(ICON_MS_STEP)) {
            lunar->pause = true;

            ps2_iop_cycle(lunar->ps2);
        }

        SeparatorText("Disassembly");

        if (BeginChild("iop##disassembly")) {
            show_iop_disassembly_view(lunar);
        } EndChild();

        End();
    }

    PopFont();
}

}