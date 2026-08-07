#include <vector>
#include <string>
#include <cctype>
#include <algorithm>
#include <regex>

#include "iris.hpp"

#include "res/IconsMaterialSymbols.h"

namespace iris {

static const char* symbols_sizing_combo_items[] = {
    ICON_MS_FIT_WIDTH " Fixed fit",
    ICON_MS_FULLSCREEN " Fixed same",
    ICON_MS_FULLSCREEN " Stretch prop",
    ICON_MS_FULLSCREEN " Stretch same"
};

static ImGuiTableFlags symbols_table_sizing_flags[] = {
    ImGuiTableFlags_SizingFixedFit,
    ImGuiTableFlags_SizingFixedSame,
    ImGuiTableFlags_SizingStretchProp,
    ImGuiTableFlags_SizingStretchSame
};

void Symbols::filter(const char* text) {
    list.clear();

    if (text[0] == '\0') {
        list = iris->debug.symbols;

        return;
    }

    std::string filter_str(text);

    for (const elf::Symbol& sym : iris->debug.symbols) {
        if (use_regex) {
            std::regex r(filter_str, std::regex::ECMAScript | (case_sensitive ? std::regex_constants::syntax_option_type(0) : std::regex::icase));

            if (std::regex_match(sym.name, r)) {
                list.push_back(sym);
            }
        } else {
            std::string sym_name_str(sym.name);

            if (!case_sensitive) {
                std::transform(sym_name_str.begin(), sym_name_str.end(), sym_name_str.begin(), tolower);
                std::transform(filter_str.begin(), filter_str.end(), filter_str.begin(), tolower);
            }

            auto it = sym_name_str.find(filter_str);

            if (it != std::string::npos) {
                list.push_back(sym);
            }
        }
    }
}

static int edit_callback(ImGuiInputTextCallbackData* data) {
    Symbols* symbols = (Symbols*)data->UserData;

    symbols->filter(data->Buf);

    return 0;
}

void Symbols::on_render() {
    using namespace ImGui;

    if (BeginMenuBar()) {
        if (imgui::BeginMenu("Settings")) {
            if (imgui::BeginMenu(ICON_MS_CROP " Sizing")) {
                for (int i = 0; i < 4; i++) {
                    if (imgui::Selectable(symbols_sizing_combo_items[i], i == table_sizing_combo)) {
                        table_sizing = symbols_table_sizing_flags[i];
                        table_sizing_combo = i;
                    }
                }

                ImGui::EndMenu();
            }

            imgui::MenuItem(ICON_MS_SEARCH_CHECK " Auto search", NULL, &autosearch);

            ImGui::EndMenu();
        }

        EndMenuBar();
    }

    SetNextItemWidth(200.0f);

    ImGuiInputFlags input_flags = ImGuiInputTextFlags_EnterReturnsTrue;

    if (autosearch) {
        input_flags |= ImGuiInputTextFlags_CallbackEdit;
    }

    if (InputTextWithHint("##search", "Search symbols...", search, sizeof(search), input_flags, edit_callback, (void*)this)) {
        filter(search);
    } SameLine();

    if (Button(ICON_MS_SEARCH)) {
        filter(search);
    } SameLine();

    if (BeginPopupContextItem("symbols_settings")) {
        if (imgui::MenuItem(ICON_MS_REGULAR_EXPRESSION " Regex mode", NULL, &use_regex)) {
            filter(search);
        }

        if (imgui::MenuItem(ICON_MS_MATCH_CASE " Case-sensitive", NULL, &case_sensitive)) {
            filter(search);
        }

        EndPopup();
    }

    if (Button(ICON_MS_SETTINGS, ImVec2(50, 0))) {
        OpenPopup("symbols_settings");
    }

    int text_height = GetTextLineHeightWithSpacing();
    ImVec2 outer_size = ImVec2(0, GetContentRegionAvail().y - text_height);

    ImGuiTableFlags table_flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_Sortable | ImGuiTableFlags_Hideable | ImGuiTableFlags_ScrollY;

    table_flags |= table_sizing;

    if (BeginTable("##symbolstb", 3, table_flags, outer_size)) {
        if (ImGuiTableSortSpecs* sort_specs = ImGui::TableGetSortSpecs()) {
            if (sort_specs->SpecsDirty) {
                if (search[0] == '\0') {
                    list = iris->debug.symbols;
                }

                // Sort by symbol
                if (sort_specs->Specs->ColumnIndex == 0) {
                    std::sort(list.begin(), list.end(), [=](const elf::Symbol& a, const elf::Symbol& b) {
                        return sort_specs->Specs->SortDirection == ImGuiSortDirection_Ascending ? std::string(a.name) < std::string(b.name) : std::string(a.name) > std::string(b.name);
                    });
                }

                // Sort by address
                if (sort_specs->Specs->ColumnIndex == 1) {
                    std::sort(list.begin(), list.end(), [=](const elf::Symbol& a, const elf::Symbol& b) {
                        return sort_specs->Specs->SortDirection == ImGuiSortDirection_Ascending ? a.addr < b.addr : a.addr > b.addr;
                    });
                }

                // Sort by size
                if (sort_specs->Specs->ColumnIndex == 2) {
                    std::sort(list.begin(), list.end(), [=](const elf::Symbol& a, const elf::Symbol& b) {
                        return sort_specs->Specs->SortDirection == ImGuiSortDirection_Ascending ? a.size < b.size : a.size > b.size;
                    });
                }

                sort_specs->SpecsDirty = false;
            }
        }

        TableSetupScrollFreeze(0, 1);
        TableSetupColumn("Symbol");
        TableSetupColumn("Address");
        TableSetupColumn("Size");
        PushFont(iris->ui.font_small_code);
        TableHeadersRow();
        PopFont();

        PushFont(iris->ui.font_code);

        int index = 0;

        for (const elf::Symbol& symbol : list) {
            TableNextRow();

            TableSetColumnIndex(0);
            Text("%s", symbol.name);
            TableSetColumnIndex(1);
            Text("%08x", symbol.addr);
            TableSetColumnIndex(2);

            char label[64];

            sprintf(label, "%llu##%d", (unsigned long long)symbol.size, index++);

            if (Selectable(label, false, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowDoubleClick)) {
                if (IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                    iris->applets.ee_control.show();
                    iris->debug.ee_control_follow_pc = false;
                    iris->debug.ee_control_address = symbol.addr;
                }
            }

            if (BeginPopupContextItem()) {
                PushFont(iris->ui.font_small_code);
                TextDisabled("%s", symbol.name);
                PopFont();

                PushFont(iris->ui.font_body);

                if (imgui::Selectable(ICON_MS_ARROW_FORWARD " Go to this address")) {
                    iris->applets.ee_control.show();
                    iris->debug.ee_control_follow_pc = false;
                    iris->debug.ee_control_address = symbol.addr;
                }

                if (imgui::Selectable(ICON_MS_ADD_CIRCLE " Set a breakpoint here")) {
                    Breakpoint b;

                    b.addr = symbol.addr;
                    b.cond_r = false;
                    b.cond_w = false;
                    b.cond_x = true;
                    b.cpu = BreakpointCpu::EE;
                    b.size = 4;
                    b.enabled = true;

                    iris->debug.breakpoints.push_back(b);
                }

                PopFont();
                EndPopup();
            }
        }

        PopFont();
        EndTable();
    }

    Text("%zu ", list.size()); SameLine(0.0, 0.0);
    TextDisabled("symbols found "); SameLine(0.0, 0.0); Text("| "); SameLine(0.0, 0.0);
    TextDisabled("%s ", use_regex ? "Regex mode" : "Normal mode"); SameLine(0.0, 0.0); Text("| "); SameLine(0.0, 0.0);
    TextDisabled("%s ", case_sensitive ? "Case-sensitive" : "Case-insensitive");
}

}
