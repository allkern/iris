#include <sstream>
#include <fstream>
#include <vector>
#include <string>
#include <cctype>

#include "iris.hpp"
#include "ee/ee_def.hpp"
#include "ee/vu_def.hpp"

#include "res/IconsMaterialSymbols.h"
#include "portable-file-dialogs.h"
#include "ps2.hpp"

namespace iris {

static const char* search_cmp_names[] = {
    "Equal",
    "Not Equal",
    "Less Than",
    "Greater Than",
    "Less Than or Equal",
    "Greater Than or Equal"
};

static const char* search_cpu_names[] = {
    "EE",
    "IOP"
};

static const char* search_type_names[] = {
    "Uint8",
    "Uint16",
    "Uint32",
    "Uint64",
    "Sint8",
    "Sint16",
    "Sint32",
    "Sint64",
    "Float32",
    "Float64"
};

template <typename T> static bool compare_values(int cmp, T a, T b) {
    switch (cmp) {
        case MemorySearch::CMP_EQUAL:
            return a == b;
        case MemorySearch::CMP_NOT_EQUAL:
            return a != b;
        case MemorySearch::CMP_LESS_THAN:
            return a < b;
        case MemorySearch::CMP_GREATER_THAN:
            return a > b;
        case MemorySearch::CMP_LESS_EQUAL:
            return a <= b;
        case MemorySearch::CMP_GREATER_EQUAL:
            return a >= b;
    }

    return false;
}

static void write_match_value(ps2::Ps2* ps2, int cpu, MemorySearch::Match& m, int type) {
    ram::Ram* mem = cpu == MemorySearch::CPU_EE ? ps2->ee_ram : ps2->iop_ram;

    switch (type) {
        case MemorySearch::TYPE_U8:
        case MemorySearch::TYPE_S8: {
            *(uint8_t*)&mem->buf[m.address] = m.curr_value.u8[0];
        } break;
        case MemorySearch::TYPE_U16:
        case MemorySearch::TYPE_S16: {
            *(uint16_t*)&mem->buf[m.address] = m.curr_value.u16[0];
        } break;
        case MemorySearch::TYPE_U32:
        case MemorySearch::TYPE_F32:
        case MemorySearch::TYPE_S32: {
            *(uint32_t*)&mem->buf[m.address] = m.curr_value.u32[0];
        } break;
        case MemorySearch::TYPE_U64:
        case MemorySearch::TYPE_F64:
        case MemorySearch::TYPE_S64: {
            *(uint64_t*)&mem->buf[m.address] = m.curr_value.u64;
        } break;
    }
}

static void sprintf_match(const MemorySearch::Value& v, char* buf, size_t size, int type, int hex) {
    switch (type) {
        case MemorySearch::TYPE_U8:
            snprintf(buf, size, hex ? "0x%02x" : "%u", v.u8[0]);
            break;
        case MemorySearch::TYPE_U16:
            snprintf(buf, size, hex ? "0x%04x" : "%u", v.u16[0]);
            break;
        case MemorySearch::TYPE_U32:
            snprintf(buf, size, hex ? "0x%08x" : "%u", v.u32[0]);
            break;
        case MemorySearch::TYPE_U64:
            snprintf(buf, size, hex ? "0x%016llx" : "%llu", v.u64);
            break;
        case MemorySearch::TYPE_S8:
            snprintf(buf, size, "%d", v.s8[0]);
            break;
        case MemorySearch::TYPE_S16:
            snprintf(buf, size, "%d", v.s16[0]);
            break;
        case MemorySearch::TYPE_S32:
            snprintf(buf, size, "%d", v.s32[0]);
            break;
        case MemorySearch::TYPE_S64:
            snprintf(buf, size, "%lld", v.s64);
            break;
        case MemorySearch::TYPE_F32:
            snprintf(buf, size, "%f", v.f32[0]);
            break;
        case MemorySearch::TYPE_F64:
            snprintf(buf, size, "%lf", v.f64);
            break;
    }
}

void MemorySearch::search(const char* value_str) {
    matches.clear();

    ram::Ram* mem = cpu == CPU_EE ? iris->ps2->ee_ram : iris->ps2->iop_ram;

    int size = 0;

    switch (type) {
        case TYPE_U8:
        case TYPE_S8:
            size = 1;
            break;
        case TYPE_U16:
        case TYPE_S16:
            size = 2;
            break;
        case TYPE_U32:
        case TYPE_S32:
        case TYPE_F32:
            size = 4;
            break;
        case TYPE_U64:
        case TYPE_S64:
        case TYPE_F64:
            size = 8;
            break;
    }

    for (int addr = 0; addr < mem->size - size; addr += (aligned ? size : 1)) {
        Match m;

        m.address = addr;
        m.curr_value.u64 = *(uint64_t*)&mem->buf[addr];
        m.prev_value = m.curr_value;
        m.cpu = cpu;
        m.type = type;

        switch (type) {
            case TYPE_U8: {
                uint8_t val = (uint8_t)std::strtoul(value_str, nullptr, 0);
                if (compare_values<uint8_t>(cmp, m.curr_value.u8[0], val)) {
                    matches.push_back(m);
                }
            } break;
            case TYPE_U16: {
                uint16_t val = (uint16_t)std::strtoul(value_str, nullptr, 0);
                if (compare_values<uint16_t>(cmp, m.curr_value.u16[0], val)) {
                    matches.push_back(m);
                }
            } break;
            case TYPE_U32: {
                uint32_t val = (uint32_t)std::strtoul(value_str, nullptr, 0);
                if (compare_values<uint32_t>(cmp, m.curr_value.u32[0], val)) {
                    matches.push_back(m);
                }
            } break;
            case TYPE_U64: {
                uint64_t val = (uint64_t)std::strtoull(value_str, nullptr, 0);
                if (compare_values<uint64_t>(cmp, m.curr_value.u64, val)) {
                    matches.push_back(m);
                }
            } break;
            case TYPE_S8: {
                int8_t val = (int8_t)std::strtol(value_str, nullptr, 0);
                if (compare_values<int8_t>(cmp, m.curr_value.s8[0], val)) {
                    matches.push_back(m);
                }
            } break;
            case TYPE_S16: {
                int16_t val = (int16_t)std::strtol(value_str, nullptr, 0);
                if (compare_values<int16_t>(cmp, m.curr_value.s16[0], val)) {
                    matches.push_back(m);
                }
            } break;
            case TYPE_S32: {
                int32_t val = (int32_t)std::strtol(value_str, nullptr, 0);
                if (compare_values<int32_t>(cmp, m.curr_value.s32[0], val)) {
                    matches.push_back(m);
                }
            } break;
            case TYPE_S64: {
                int64_t val = (int64_t)std::strtoll(value_str, nullptr, 0);
                if (compare_values<int64_t>(cmp, m.curr_value.s64, val)) {
                    matches.push_back(m);
                }
            } break;
            case TYPE_F32: {
                float val = std::strtof(value_str, nullptr);
                if (compare_values<float>(cmp, m.curr_value.f32[0], val)) {
                    matches.push_back(m);
                }
            } break;
            case TYPE_F64: {
                double val = std::strtod(value_str, nullptr);
                if (compare_values<double>(cmp, m.curr_value.f64, val)) {
                    matches.push_back(m);
                }
            } break;
        }
    }
}

void MemorySearch::filter(const char* value_str) {
    switch (type) {
        case TYPE_U8: {
            uint8_t val = (uint8_t)std::strtoul(value_str, nullptr, 0);

            std::erase_if(matches, [this, val](const Match& m) {
                return !compare_values<uint8_t>(cmp, m.curr_value.u8[0], val);
            });
        } break;

        case TYPE_U16: {
            uint16_t val = (uint16_t)std::strtoul(value_str, nullptr, 0);

            std::erase_if(matches, [this, val](const Match& m) {
                return !compare_values<uint16_t>(cmp, m.curr_value.u16[0], val);
            });
        } break;

        case TYPE_U32: {
            uint32_t val = (uint32_t)std::strtoul(value_str, nullptr, 0);

            std::erase_if(matches, [this, val](const Match& m) {
                return !compare_values<uint32_t>(cmp, m.curr_value.u32[0], val);
            });
        } break;

        case TYPE_U64: {
            uint64_t val = (uint64_t)std::strtoull(value_str, nullptr, 0);

            std::erase_if(matches, [this, val](const Match& m) {
                return !compare_values<uint64_t>(cmp, m.curr_value.u64, val);
            });
        } break;

        case TYPE_S8: {
            int8_t val = (int8_t)std::strtol(value_str, nullptr, 0);

            std::erase_if(matches, [this, val](const Match& m) {
                return !compare_values<int8_t>(cmp, m.curr_value.s8[0], val);
            });
        } break;

        case TYPE_S16: {
            int16_t val = (int16_t)std::strtol(value_str, nullptr, 0);

            std::erase_if(matches, [this, val](const Match& m) {
                return !compare_values<int16_t>(cmp, m.curr_value.s16[0], val);
            });
        } break;

        case TYPE_S32: {
            int32_t val = (int32_t)std::strtol(value_str, nullptr, 0);

            std::erase_if(matches, [this, val](const Match& m) {
                return !compare_values<int32_t>(cmp, m.curr_value.s32[0], val);
            });
        } break;

        case TYPE_S64: {
            int64_t val = (int64_t)std::strtoll(value_str, nullptr, 0);

            std::erase_if(matches, [this, val](const Match& m) {
                return !compare_values<int64_t>(cmp, m.curr_value.s64, val);
            });
        } break;

        case TYPE_F32: {
            float val = std::strtof(value_str, nullptr);

            std::erase_if(matches, [this, val](const Match& m) {
                return !compare_values<float>(cmp, m.curr_value.f32[0], val);
            });
        } break;

        case TYPE_F64: {
            double val = std::strtod(value_str, nullptr);

            std::erase_if(matches, [this, val](const Match& m) {
                return !compare_values<double>(cmp, m.curr_value.f64, val);
            });
        } break;
    }
}

void MemorySearch::update_matches() {
    if (update_counter != 4) {
        update_counter++;

        return;
    }

    for (Match& m : matches) {
        ram::Ram* mem = m.cpu == CPU_EE ? iris->ps2->ee_ram : iris->ps2->iop_ram;

        m.curr_value.u64 = *(uint64_t*)&mem->buf[m.address];
    }

    for (Match& m : address_list) {
        ram::Ram* mem = m.cpu == CPU_EE ? iris->ps2->ee_ram : iris->ps2->iop_ram;

        m.curr_value.u64 = *(uint64_t*)&mem->buf[m.address];
    }

    update_counter = 0;
}

void MemorySearch::show_match_change_dialog(Match& m, char* label, int value_type, int value_cpu) {
    using namespace ImGui;

    PushFont(iris->ui.font_small);
    TextDisabled("Edit "); SameLine(0.0, 0.0);
    PushFont(iris->ui.font_small_code);
    Text("%s", label);
    PopFont();
    PopFont();

    PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6.0, 2.0));
    PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0, 8.0));

    PushFont(iris->ui.font_body);
    PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.35, 0.35, 0.35, 0.35));

    AlignTextToFramePadding();
    Text(ICON_MS_EDIT); SameLine();

    SetNextItemWidth(100);
    PushFont(iris->ui.font_code);

    bool commit = InputText("##", edit_buf, sizeof(edit_buf), ImGuiInputTextFlags_EnterReturnsTrue);

    PopFont();

    if (Button("Change"))
        commit = true;

    SameLine();

    if (commit) {
        if (edit_buf[0]) {
            if (strchr(edit_buf, '.')) {
                if (value_type == TYPE_F64) {
                    m.curr_value.f64 = std::strtod(edit_buf, nullptr);
                } else {
                    m.curr_value.f32[0] = std::strtof(edit_buf, nullptr);
                }
            } else {
                m.curr_value.u64 = std::strtoull(edit_buf, nullptr, 0);
            }

            write_match_value(iris->ps2, value_cpu, m, value_type);
        }

        CloseCurrentPopup();
    }

    if (Button("Cancel"))
        CloseCurrentPopup();

    PopStyleColor();
    PopStyleVar(2);

    PopFont();
}

void MemorySearch::show_description_change_dialog(Match& m) {
    using namespace ImGui;

    PushFont(iris->ui.font_small);
    TextDisabled("Edit description");
    PopFont();

    PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6.0, 2.0));
    PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0, 8.0));

    PushFont(iris->ui.font_body);
    PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.35, 0.35, 0.35, 0.35));

    AlignTextToFramePadding();
    Text(ICON_MS_EDIT); SameLine();

    SetNextItemWidth(100);

    if (desc_buf[0] == '\0') {
        strncpy(desc_buf, m.description.c_str(), sizeof(desc_buf));
    }

    desc_buf[sizeof(desc_buf) - 1] = '\0';

    if (InputText("##desc", desc_buf, sizeof(desc_buf), ImGuiInputTextFlags_EnterReturnsTrue)) {
        m.description = "\"" + std::string(desc_buf) + "\"";

        CloseCurrentPopup();
    }

    if (Button("Change")) {
        m.description = "\"" + std::string(desc_buf) + "\"";

        CloseCurrentPopup();
    } SameLine();

    if (Button("Cancel"))
        CloseCurrentPopup();

    PopStyleColor();
    PopStyleVar(2);

    PopFont();
}

void MemorySearch::show_search_table() {
    using namespace ImGui;

    if (matches.empty()) {
        SeparatorText("Search results");
    } else {
        char buf[256];

        if (matches.size() > 9999) {
            snprintf(buf, sizeof(buf), "Search results (9999+ matches)");
        } else {
            snprintf(buf, sizeof(buf), "Search results (%zu matches)", matches.size());
        }

        SeparatorText(buf);
    }

    if (BeginTable("Matches", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingStretchProp)) {
        TableSetupColumn("Address");
        TableSetupColumn("Previous Value");
        TableSetupColumn("Current Value");
        TableHeadersRow();

        for (Match& m : matches) {
            TableNextRow();

            TableSetColumnIndex(0);

            char addr_label[16];
            char prev_value_label[64];
            char curr_value_label[64];

            sprintf(addr_label, "0x%08x", m.address);
            sprintf_match(m.prev_value, prev_value_label, sizeof(prev_value_label), type, display_hex);
            sprintf_match(m.curr_value, curr_value_label, sizeof(curr_value_label), type, display_hex);

            PushFont(iris->ui.font_code);

            Selectable(addr_label, false, ImGuiSelectableFlags_AllowOverlap | ImGuiSelectableFlags_SpanAllColumns);

            if (IsItemClicked(ImGuiMouseButton_Right)) {
                OpenPopup("context_menu");

                selected_match = m.address;
            }

            if (IsItemHovered() && IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                OpenPopup("edit_value_popup");

                selected_match = m.address;
            }

            if (selected_match == m.address) if (BeginPopup("context_menu")) {
                PushFont(iris->ui.font_small_code);
                TextDisabled("0x%08x", m.address);
                PopFont();

                PushFont(iris->ui.font_body);
                if (BeginMenu(ICON_MS_CONTENT_COPY " Copy")) {
                    if (Selectable("Address")) {
                        ImGui::SetClipboardText(addr_label);
                    }

                    if (Selectable("Previous Value")) {
                        ImGui::SetClipboardText(prev_value_label);
                    }

                    if (Selectable("Current Value")) {
                        ImGui::SetClipboardText(curr_value_label);
                    }

                    ImGui::EndMenu();
                }

                if (BeginMenu(ICON_MS_EDIT " Edit value")) {
                    show_match_change_dialog(m, addr_label, type, cpu);

                    ImGui::EndMenu();
                }

                bool in_address_list = false;

                for (const Match& am : address_list) {
                    if (am.address == m.address) {
                        in_address_list = true;
                        break;
                    }
                }

                if (Selectable(in_address_list ? ICON_MS_REMOVE " Remove from address list" : ICON_MS_ADD " Add to address list")) {
                    if (in_address_list) {
                        std::erase_if(address_list, [m](const Match& am) {
                            return am.address == m.address;
                        });
                    } else {
                        m.description = "\"No description\"";
                        m.cpu = cpu;
                        m.type = type;

                        address_list.push_back(m);
                    }
                }

                PopFont();

                EndPopup();
            }

            if (selected_match == m.address) if (BeginPopup("edit_value_popup")) {
                show_match_change_dialog(m, addr_label, type, cpu);

                EndPopup();
            }

            TableSetColumnIndex(1);

            Text("%s", prev_value_label);

            TableSetColumnIndex(2);

            Text("%s", curr_value_label);

            PopFont();
        }

        EndTable();
    }
}

void MemorySearch::show_address_list() {
    using namespace ImGui;

    if (BeginTable("Addresses", 6, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable)) {
        TableSetupColumn("Address", ImGuiTableColumnFlags_WidthFixed, 100.0f);
        TableSetupColumn("CPU", ImGuiTableColumnFlags_WidthFixed, 20.0f);
        TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 30.0f);
        TableSetupColumn("Description");
        TableSetupColumn("Previous Value", ImGuiTableColumnFlags_WidthFixed, 150.0f);
        TableSetupColumn("Current Value");
        TableHeadersRow();

        for (Match& m : address_list) {
            TableNextRow();

            TableSetColumnIndex(0);

            char addr_label[16];
            char prev_value_label[64];
            char curr_value_label[64];

            sprintf(addr_label, "0x%08x", m.address);
            sprintf_match(m.prev_value, prev_value_label, sizeof(prev_value_label), m.type, display_hex);
            sprintf_match(m.curr_value, curr_value_label, sizeof(curr_value_label), m.type, display_hex);

            PushFont(iris->ui.font_code);

            Selectable(addr_label, false, ImGuiSelectableFlags_AllowOverlap | ImGuiSelectableFlags_SpanAllColumns);

            if (IsItemClicked(ImGuiMouseButton_Right)) {
                OpenPopup("context_menu_al");

                selected_address = m.address;
            }

            if (IsItemHovered() && IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                OpenPopup("edit_value_popup_al");

                selected_address = m.address;
            }

            if (selected_address == m.address) if (BeginPopup("context_menu_al")) {
                PushFont(iris->ui.font_small_code);
                TextDisabled("0x%08x", m.address);
                PopFont();

                PushFont(iris->ui.font_body);
                if (BeginMenu(ICON_MS_CONTENT_COPY " Copy")) {
                    if (Selectable("Address")) {
                        ImGui::SetClipboardText(addr_label);
                    }

                    if (Selectable("Previous Value")) {
                        ImGui::SetClipboardText(prev_value_label);
                    }

                    if (Selectable("Current Value")) {
                        ImGui::SetClipboardText(curr_value_label);
                    }

                    ImGui::EndMenu();
                }

                if (BeginMenu(ICON_MS_EDIT " Edit value")) {
                    show_match_change_dialog(m, addr_label, m.type, m.cpu);

                    ImGui::EndMenu();
                }

                if (BeginMenu(ICON_MS_EDIT " Edit description")) {
                    show_description_change_dialog(m);

                    ImGui::EndMenu();
                }

                if (BeginMenu(ICON_MS_EDIT " Edit type")) {
                    for (int i = 0; i < IM_ARRAYSIZE(search_type_names); i++) {
                        if (MenuItem(search_type_names[i], nullptr, m.type == i)) {
                            m.type = i;
                        }
                    }

                    ImGui::EndMenu();
                }

                if (Selectable(ICON_MS_REMOVE " Remove from address list")) {
                    std::erase_if(address_list, [m](const Match& am) {
                        return am.address == m.address;
                    });
                }

                PopFont();

                EndPopup();
            }

            if (selected_address == m.address) if (BeginPopup("edit_value_popup_al")) {
                show_match_change_dialog(m, addr_label, m.type, m.cpu);

                EndPopup();
            }

            TableSetColumnIndex(1);

            Text("%s", m.cpu == CPU_EE ? "EE" : "IOP");

            TableSetColumnIndex(2);

            Text("%s", search_type_names[m.type]);

            TableSetColumnIndex(3);

            PushFont(iris->ui.font_body);
            Text("%s", m.description.c_str());
            PopFont();

            TableSetColumnIndex(4);

            Text("%s", prev_value_label);

            TableSetColumnIndex(5);

            Text("%s", curr_value_label);

            PopFont();
        }

        EndTable();
    }
}

void MemorySearch::show_search_options() {
    using namespace ImGui;

    SeparatorText("Search options");

    PushItemWidth(-FLT_MIN);

    Text("Type");
    if (BeginCombo("##type", search_type_names[type])) {
        for (int i = 0; i < IM_ARRAYSIZE(search_type_names); i++) {
            if (Selectable(search_type_names[i], type == i)) {
                type = i;
            }
        }

        EndCombo();
    }

    PushStyleVarY(ImGuiStyleVar_FramePadding, 2.0f);
    Checkbox("Aligned", &aligned);
    PopStyleVar();

    Text("Comparison");
    if (BeginCombo("##comparison", search_cmp_names[cmp])) {
        for (int i = 0; i < IM_ARRAYSIZE(search_cmp_names); i++) {
            if (Selectable(search_cmp_names[i], cmp == i)) {
                cmp = i;
            }
        }

        EndCombo();
    }

    Text("Memory");
    if (BeginCombo("##memory", search_cpu_names[cpu])) {
        for (int i = 0; i < IM_ARRAYSIZE(search_cpu_names); i++) {
            if (Selectable(search_cpu_names[i], cpu == i)) {
                cpu = i;
            }
        }

        EndCombo();
    }

    Text("Value");
    if (InputText("##value", value_buf, sizeof(value_buf), ImGuiInputTextFlags_EnterReturnsTrue)) {
        search(value_buf);
    }

    PopItemWidth();

    BeginDisabled(value_buf[0] == '\0');
    if (Button("Search")) {
        search(value_buf);
    } SameLine();
    EndDisabled();

    BeginDisabled(value_buf[0] == '\0' || matches.empty());
    if (Button("Filter")) {
        filter(value_buf);
    }
    EndDisabled();
}

std::string MemorySearch::serialize_address_list() {
    std::stringstream ss;

    for (const Match& m : address_list) {
        ss << "0x" << std::hex << m.address << "," << m.description << "," << m.cpu << "," << m.type << "\n";
    }

    return ss.str();
}

void MemorySearch::import_address_list_from_stream(std::istream& stream) {
    address_list.clear();

    std::string line;

    while (std::getline(stream, line)) {
        std::stringstream linestream(line);
        std::string address_str;
        std::string description;
        std::string cpu_str;
        std::string type_str;

        bool valid = true;

        valid = std::getline(linestream, address_str, ',') &&
                std::getline(linestream, description, ',') &&
                std::getline(linestream, cpu_str, ',') &&
                std::getline(linestream, type_str);

        if (valid) {
            Match m;

            m.address = (uint32_t)std::strtoul(address_str.c_str(), nullptr, 0);
            m.description = description;
            m.prev_value.u64 = 0;
            m.curr_value.u64 = 0;
            m.cpu = std::strtoul(cpu_str.c_str(), nullptr, 0);
            m.type = std::strtoul(type_str.c_str(), nullptr, 0);

            // To-do: Remove double quotes from description if present

            address_list.push_back(m);
        }
    }
}

bool MemorySearch::begin() {
    update_matches();

    ImGui::SetNextWindowSizeConstraints(ImVec2(600, 560), ImVec2(FLT_MAX, FLT_MAX));

    return Applet::begin();
}

void MemorySearch::on_render() {
    using namespace ImGui;

    if (BeginMenuBar()) {
        if (BeginMenu("File")) {
            if (BeginMenu("Address list")) {
                if (MenuItem("Export to file...")) {
                    std::string str = serialize_address_list();

                    pfd::save_file file("Export Address List", "address_list.csv", { "CSV Files (*.csv)", "*.csv", "All files (*.*)", "" });

                    if (!file.result().empty()) {
                        FILE* f = fopen(file.result().c_str(), "w");

                        if (f) {
                            fwrite(str.c_str(), 1, str.size(), f);

                            fclose(f);
                        } else {
                            push_info(iris, "Failed to open file for writing address list");
                        }
                    }
                }

                if (MenuItem("Import from file...")) {
                    pfd::open_file file("Import Address List", "", { "CSV Files (*.csv)", "*.csv", "All Files (*.*)", "*" });

                    if (!file.result().empty()) {
                        std::ifstream f(file.result().at(0));

                        if (f.is_open()) {
                            import_address_list_from_stream(f);
                        } else {
                            push_info(iris, "Failed to open file for reading address list");
                        }
                    }
                }

                if (MenuItem("Export to clipboard")) {
                    std::string str = serialize_address_list();

                    SDL_SetClipboardText(str.c_str());
                }

                if (MenuItem("Import from clipboard")) {
                    if (SDL_HasClipboardText()) {
                        std::string clip_text = SDL_GetClipboardText();

                        std::istringstream iss(clip_text);

                        import_address_list_from_stream(iss);
                    }
                }

                if (MenuItem("Clear")) {
                    address_list.clear();
                }

                ImGui::EndMenu();
            }

            ImGui::EndMenu();
        }

        if (BeginMenu("View")) {
            MenuItem("Display as hex", nullptr, &display_hex);

            ImGui::EndMenu();
        }

        EndMenuBar();
    }

    if (BeginChild("##search_table", ImVec2(GetContentRegionAvail().x - 225, GetContentRegionAvail().y - 220))) {
        show_search_table();
    } EndChild(); SameLine();

    if (BeginChild("##search_options", ImVec2(0, GetContentRegionAvail().y - 220))) {
        show_search_options();
    } EndChild();

    SeparatorText("Address list");

    if (BeginChild("##address_list")) {
        show_address_list();
    } EndChild();
}

}
