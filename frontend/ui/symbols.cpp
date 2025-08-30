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

static int symbols_table_sizing_combo = 0;
static int symbols_table_sizing = ImGuiTableFlags_SizingStretchProp;

std::vector <iris::elf_symbol> symbols_list;

bool regex = false;
bool case_sensitive = false;
bool autosearch = true;

void filter_symbols(iris::instance* iris, const char* filter, bool regex, bool case_sensitive) {
    symbols_list.clear();

    if (filter[0] == '\0') {
        symbols_list = iris->symbols;

        return;
    }

    std::string filter_str(filter);

    for (const iris::elf_symbol& sym : iris->symbols) {
        if (regex) {
            std::regex r(filter_str, std::regex::ECMAScript | (case_sensitive ? std::regex_constants::syntax_option_type(0) : std::regex::icase));

            if (std::regex_match(sym.name, r)) {
                symbols_list.push_back(sym);
            }
        } else {
            std::string sym_name_str(sym.name);

            if (!case_sensitive) {
                std::transform(sym_name_str.begin(), sym_name_str.end(), sym_name_str.begin(), tolower);
                std::transform(filter_str.begin(), filter_str.end(), filter_str.begin(), tolower);
            }

            auto it = sym_name_str.find(filter_str);

            if (it != std::string::npos) {
                symbols_list.push_back(sym);
            }
        }
    }
}

int edit_callback(ImGuiInputTextCallbackData* data) {
    iris::instance* iris = (iris::instance*)data->UserData;

    filter_symbols(iris, data->Buf, regex, case_sensitive);

    return 0;
}

void show_symbols(iris::instance* iris) {
    using namespace ImGui;

    if (Begin("Symbols", &iris->show_symbols, ImGuiWindowFlags_MenuBar)) {
        if (BeginMenuBar()) {
            if (BeginMenu("Settings")) {
                if (BeginMenu(ICON_MS_CROP " Sizing")) {
                    for (int i = 0; i < 4; i++) {
                        if (Selectable(symbols_sizing_combo_items[i], i == symbols_table_sizing_combo)) {
                            symbols_table_sizing = symbols_table_sizing_flags[i];
                            symbols_table_sizing_combo = i;
                        }
                    }

                    ImGui::EndMenu();
                }

                MenuItem(ICON_MS_SEARCH_CHECK " Auto search", NULL, &autosearch);

                EndMenu();
            }

            EndMenuBar();
        }

        static char buf[512];

        SetNextItemWidth(200.0f);

        ImGuiInputFlags flags = ImGuiInputTextFlags_EnterReturnsTrue;

        if (autosearch) {
            flags |= ImGuiInputTextFlags_CallbackEdit;
        }

        if (InputTextWithHint("##search", "Search symbols...", buf, 512, flags, edit_callback, (void*)iris)) {
            filter_symbols(iris, buf, regex, case_sensitive);
        } SameLine();

        if (Button(ICON_MS_SEARCH)) {
            filter_symbols(iris, buf, regex, case_sensitive);
        } SameLine();

        if (BeginPopupContextItem("symbols_settings")) {
            if (MenuItem(ICON_MS_REGULAR_EXPRESSION " Regex mode", NULL, &regex)) {
                filter_symbols(iris, buf, regex, case_sensitive);
            }

            if (MenuItem(ICON_MS_MATCH_CASE " Case-sensitive", NULL, &case_sensitive)) {
                filter_symbols(iris, buf, regex, case_sensitive);
            }

            EndPopup();
        }

        if (Button(ICON_MS_SETTINGS, ImVec2(50, 0))) {
            OpenPopup("symbols_settings");
        }

        int text_height = GetTextLineHeightWithSpacing();
        ImVec2 outer_size = ImVec2(0, GetContentRegionAvail().y - text_height);

        ImGuiTableFlags table_flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_Sortable | ImGuiTableFlags_Hideable | ImGuiTableFlags_ScrollY;

        table_flags |= symbols_table_sizing;

        if (BeginTable("##symbolstb", 3, table_flags, outer_size)) {
            if (ImGuiTableSortSpecs* sort_specs = ImGui::TableGetSortSpecs()) {
                if (sort_specs->SpecsDirty) {
                    if (buf[0] == '\0') {
                        symbols_list = iris->symbols;
                    }

                    // Sort by symbol
                    if (sort_specs->Specs->ColumnIndex == 0) {
                        std::sort(symbols_list.begin(), symbols_list.end(), [=](const iris::elf_symbol& a, const iris::elf_symbol& b) {
                            return sort_specs->Specs->SortDirection == ImGuiSortDirection_Ascending ? std::string(a.name) < std::string(b.name) : std::string(a.name) > std::string(b.name);
                        });
                    }

                    // Sort by address
                    if (sort_specs->Specs->ColumnIndex == 1) {
                        std::sort(symbols_list.begin(), symbols_list.end(), [=](const iris::elf_symbol& a, const iris::elf_symbol& b) {
                            return sort_specs->Specs->SortDirection == ImGuiSortDirection_Ascending ? a.addr < b.addr : a.addr > b.addr;
                        });
                    }

                    // Sort by size
                    if (sort_specs->Specs->ColumnIndex == 2) {
                        std::sort(symbols_list.begin(), symbols_list.end(), [=](const iris::elf_symbol& a, const iris::elf_symbol& b) {
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
            PushFont(iris->font_small_code);
            TableHeadersRow();
            PopFont();

            PushFont(iris->font_code);

            int index = 0;

            for (const iris::elf_symbol& symbol : symbols_list) {
                TableNextRow();

                TableSetColumnIndex(0);
                Text("%s", symbol.name);
                TableSetColumnIndex(1);
                Text("%08x", symbol.addr);
                TableSetColumnIndex(2);

                char label[64];

                sprintf(label, "%zu##%d", symbol.size, index++);

                if (Selectable(label, false, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowDoubleClick)) {
                    if (IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                        iris->show_ee_control = true;
                        iris->ee_control_follow_pc = false;
                        iris->ee_control_address = symbol.addr;
                    }
                }

                if (BeginPopupContextItem()) {
                    PushFont(iris->font_small_code);
                    TextDisabled("%s", symbol.name);
                    PopFont();

                    PushFont(iris->font_body);

                    if (Selectable(ICON_MS_ARROW_RIGHT " Go to this address")) {
                        iris->show_ee_control = true;
                        iris->ee_control_follow_pc = false;
                        iris->ee_control_address = symbol.addr;
                    }

                    if (Selectable(ICON_MS_CIRCLE " Set a breakpoint here")) {
                        breakpoint b;

                        b.addr = symbol.addr;
                        b.cond_r = false;
                        b.cond_w = false;
                        b.cond_x = true;
                        b.cpu = BKPT_CPU_EE;
                        b.size = 4;
                        b.enabled = true;

                        iris->breakpoints.push_back(b);
                    }

                    PopFont();
                    EndPopup();
                }
            }

            PopFont();
            EndTable();
        }

        Text("%zu ", symbols_list.size()); SameLine(0.0, 0.0);
        TextDisabled("symbols found "); SameLine(0.0, 0.0); Text("| "); SameLine(0.0, 0.0);
        TextDisabled("%s ", regex ? "Regex mode" : "Normal mode"); SameLine(0.0, 0.0); Text("| "); SameLine(0.0, 0.0);
        TextDisabled("%s ", case_sensitive ? "Case-sensitive" : "Case-insensitive");
    } End();
}

}