#include <algorithm>
#include <vector>
#include <string>
#include <cctype>

#include "iris.hpp"

#include "pfd/pfd.h"
#include "res/IconsMaterialSymbols.h"

#include "ee/vu_dis.h"

#include <algorithm> 
#include <cctype>
#include <locale>

// Trim from the start (in place)
inline void ltrim_impl(std::string &s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) {
        return !std::isspace(ch);
    }));
}

// Trim from the end (in place)
inline void rtrim_impl(std::string &s) {
    s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) {
        return !std::isspace(ch);
    }).base(), s.end());
}

// Trim from both ends (in place)
inline void trim_impl(std::string &s) {
    rtrim_impl(s);
    ltrim_impl(s);
}

// Trim from the start (copying)
inline std::string ltrim(std::string s) {
    ltrim_impl(s);
    return s;
}

// Trim from the end (copying)
inline std::string rtrim(std::string s) {
    rtrim_impl(s);
    return s;
}

// Trim from both ends (copying)
inline std::string trim(std::string s) {
    trim_impl(s);
    return s;
}

#define IM_RGB(r, g, b) ImVec4(((float)r / 255.0f), ((float)g / 255.0f), ((float)b / 255.0f), 1.0)

namespace iris {

struct vu_dis_state g_vu_dis_state = { 0 };

uint32_t addr = 0;
bool stop_at_e_bit = false;
bool disassemble_all = false;
bool add_padding = true;
bool compact_view = false;
bool show_address_opcode = true;

void print_highlighted_vu1(const char* buf) {
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
        } else if (*buf == '+') {
            text.push_back(*buf++);
        } else if (*buf == '-') {
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
        if (t[0] == 'v' && (t[1] == 'f' || t[1] == 'i')) {
            TextColored(IM_RGB(68, 169, 240), "%s", t.c_str());
        } else if (rtrim(t) == "acc") {
            TextColored(IM_RGB(68, 169, 240), "%s", t.c_str());
        } else if (rtrim(t) == "q") {
            TextColored(IM_RGB(68, 169, 240), "%s", t.c_str());
        } else if (rtrim(t) == "p") {
            TextColored(IM_RGB(68, 169, 240), "%s", t.c_str());
        } else if (rtrim(t) == "i") {
            TextColored(IM_RGB(68, 169, 240), "%s", t.c_str());
        } else if (rtrim(t) == "r") {
            TextColored(IM_RGB(68, 169, 240), "%s", t.c_str());
        } else if (isalpha(t[0])) {
            TextColored(IM_RGB(211, 167, 30), "%s", t.c_str());
        } else if (isdigit(t[0]) || t[0] == '-') {
            TextColored(IM_RGB(138, 143, 226), "%s", t.c_str());
        } else if (t[0] == '<') {
            TextColored(IM_RGB(89, 89, 89), "%s", t.c_str());
        } else {
            Text("%s", t.c_str());
        }

        SameLine(0.0f, 0.0f);
    }

    NewLine();
}

static void show_vu_disassembly_view(iris::instance* iris, uint64_t* mem, size_t size) {
    using namespace ImGui;

    PushFont(iris->font_code);

    if (BeginTable("table1", compact_view ? 2 : 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_Hideable)) {
        PushFont(iris->font_small_code);

        TableSetupColumn(" Address/Opcode");
        TableSetupColumn(compact_view ? "Upper/Lower" : "Upper");

        if (!compact_view) {
            TableSetupColumn("Lower");
        }

        TableHeadersRow();
        PopFont();

        int e_bit = 0;

        for (int row = disassemble_all ? 0 : addr; row < size; row++) {
            g_vu_dis_state.addr = row * 8;

            TableNextRow();
            TableSetColumnIndex(0);

            uint64_t u = mem[row] >> 32;
            uint64_t l = mem[row] & 0xffffffff;

            if (!compact_view) {
                TextDisabled("%04x: %08x %08x", row, u, l); SameLine();
            } else {
                TextDisabled("%04x: %08x", row, u); SameLine();
            }

            TableSetColumnIndex(1);

            char upper[512], lower[512];

            g_vu_dis_state.addr = row;

            vu_disassemble_upper(upper, u, &g_vu_dis_state);
            vu_disassemble_lower(lower, l, &g_vu_dis_state);

            if (add_padding && !compact_view) {
#ifdef _WIN32
                sprintf_s(upper, "%-40s", upper);
                sprintf_s(lower, "%-40s", lower);
#else
                sprintf(upper, "%-40s", upper);
                sprintf(lower, "%-40s", lower);
#endif
            }

            print_highlighted_vu1(upper);

            if (!compact_view) {
                TableSetColumnIndex(2);
            }

            print_highlighted_vu1(lower);

            if (e_bit && stop_at_e_bit && !disassemble_all) break;

            e_bit = (u & 0x40000000) ? 1 : 0;
        }

        EndTable();
    }

    PopFont();
}

void save_disassembly(FILE* file, uint64_t* mem, size_t size) {
    int e_bit = 0;

    for (int row = disassemble_all ? 0 : addr; row < size; row++) {
        g_vu_dis_state.addr = row * 8;

        uint64_t u = mem[row] >> 32;
        uint64_t l = mem[row] & 0xffffffff;

        char upper[512], lower[512];

        g_vu_dis_state.addr = row;

        vu_disassemble_upper(upper, u, &g_vu_dis_state);
        vu_disassemble_lower(lower, l, &g_vu_dis_state);

        if (add_padding && !compact_view) {
#ifdef _WIN32
            sprintf_s(upper, "%-40s", upper);
            sprintf_s(lower, "%-40s", lower);
#else
            sprintf(upper, "%-40s", upper);
            sprintf(lower, "%-40s", lower);
#endif
        }

        if (compact_view) {
            fprintf(file, "%04x: %08x %s\n", row, u, upper);
            fprintf(file, "      %08x %s\n", l, lower);
        } else {
            fprintf(file, "%04x: %08x %08x %s %s\n", row, u, l, upper, lower);
        }

        if (e_bit && stop_at_e_bit && !disassemble_all) break;

        e_bit = (u & 0x40000000) ? 1 : 0;
    }
}

void show_vu_disassembler(iris::instance* iris) {
    using namespace ImGui;

    PushFont(iris->font_icons);

    if (Begin("VU disassembler", &iris->show_vu_disassembler, ImGuiWindowFlags_MenuBar)) {
        if (BeginMenuBar()) {
            if (BeginMenu("File")) {
                if (MenuItem(ICON_MS_FILE_SAVE " Save disassembly as...", NULL)) {
                    
                }

                ImGui::EndMenu();
            }

            if (BeginMenu("Settings")) {
                MenuItem(ICON_MS_FORMAT_LETTER_SPACING_WIDER " Add padding", NULL, &add_padding);
                MenuItem(ICON_MS_COLLAPSE_ALL " Compact view", NULL, &compact_view);

                ImGui::EndMenu();
            }

            EndMenuBar();
        }

        if (BeginTabBar("##vudistabbar", ImGuiTabBarFlags_Reorderable)) {
            if (BeginTabItem("VU0")) {
                BeginDisabled(disassemble_all);
                AlignTextToFramePadding();
                Text("Address"); SameLine();
                
                SetNextItemWidth(100.0f);
                PushFont(iris->font_code);

                if (InputInt("##address", (int*)&addr, 0, 0, ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_EscapeClearsAll));

                PopFont();

                SameLine();
                Checkbox("Stop at E bit", &stop_at_e_bit);

                EndDisabled();

                SameLine();
                Checkbox("Disassemble all", &disassemble_all); SameLine();
                if (Button(ICON_MS_SAVE)) {
                    pfd::save_file file("Save VU0 disassembly", "vu0.s", { "Text files", "*.txt" });

                    if (!file.result().empty()) {
                        FILE* f = fopen(file.result().c_str(), "w");

                        if (f) {
                            save_disassembly(f, iris->ps2->vu0->micro_mem, 512);
                            fclose(f);
                        } else {
                            pfd::message("Error", "Failed to open file for writing.", pfd::choice::ok, pfd::icon::error);
                        }
                    }
                } SameLine();

                BeginDisabled(!stop_at_e_bit);
                if (Button(ICON_MS_ARROW_RIGHT_ALT " Next program")) {
                    int prev = addr;

                    addr = 0;

                    for (int i = prev; i < 512; i++) {
                        uint32_t upper = iris->ps2->vu0->micro_mem[i] >> 32;

                        if (upper & 0x40000000) {
                            addr = i + 2;

                            break;
                        }
                    }
                }
                EndDisabled();

                SeparatorText("Disassembly");

                if (BeginChild("vu0##disassembly")) {
                    show_vu_disassembly_view(iris, iris->ps2->vu0->micro_mem, 512);
                } EndChild();
            
                EndTabItem();
            }

            if (BeginTabItem("VU1")) {
                BeginDisabled(disassemble_all);
                AlignTextToFramePadding();
                Text("Address"); SameLine();

                SetNextItemWidth(100.0f);
                PushFont(iris->font_code);

                if (InputInt("##address", (int*)&addr, 0, 0, ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_EscapeClearsAll));

                PopFont();

                SameLine();
                Checkbox("Stop at E bit", &stop_at_e_bit);

                EndDisabled();

                SameLine();
                Checkbox("Disassemble all", &disassemble_all); SameLine();
                if (Button(ICON_MS_SAVE)) {
                    pfd::save_file file("Save VU1 disassembly", "vu1.s", { "Text files", "*.txt" });

                    if (!file.result().empty()) {
                        FILE* f = fopen(file.result().c_str(), "w");

                        if (f) {
                            save_disassembly(f, iris->ps2->vu1->micro_mem, 2048);
                            fclose(f);
                        } else {
                            pfd::message("Error", "Failed to open file for writing.", pfd::choice::ok, pfd::icon::error);
                        }
                    }
                } SameLine();

                BeginDisabled(!stop_at_e_bit);
                if (Button(ICON_MS_ARROW_RIGHT_ALT " Next program")) {
                    int prev = addr;

                    addr = 0;

                    for (int i = prev; i < 2048; i++) {
                        uint32_t upper = iris->ps2->vu1->micro_mem[i] >> 32;

                        if (upper & 0x40000000) {
                            addr = i + 2;

                            break;
                        }
                    }
                }
                EndDisabled();

                SeparatorText("Disassembly");

                if (BeginChild("vu1##disassembly")) {
                    show_vu_disassembly_view(iris, iris->ps2->vu1->micro_mem, 2048);
                } EndChild();

                EndTabItem();
            }

            EndTabBar();
        }
    } End();

    PopFont();
}

}