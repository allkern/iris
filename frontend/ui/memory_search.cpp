#include <sstream>
#include <fstream>
#include <vector>
#include <string>
#include <cctype>

#include "iris.hpp"
#include "ee/ee_def.hpp"

#include "res/IconsMaterialSymbols.h"
#include "portable-file-dialogs.h"

namespace iris {

enum {
    SEARCH_CMP_EQUAL,
    SEARCH_CMP_NOT_EQUAL,
    SEARCH_CMP_LESS_THAN,
    SEARCH_CMP_GREATER_THAN,
    SEARCH_CMP_LESS_EQUAL,
    SEARCH_CMP_GREATER_EQUAL
};

const char* search_cmp_names[] = {
    "Equal",
    "Not Equal",
    "Less Than",
    "Greater Than",
    "Less Than or Equal",
    "Greater Than or Equal"
};

enum {
    SEARCH_CPU_EE,
    SEARCH_CPU_IOP
};

const char* search_cpu_names[] = {
    "EE",
    "IOP"
};

enum {
    SEARCH_TYPE_U8,
    SEARCH_TYPE_U16,
    SEARCH_TYPE_U32,
    SEARCH_TYPE_U64,
    SEARCH_TYPE_S8,
    SEARCH_TYPE_S16,
    SEARCH_TYPE_S32,
    SEARCH_TYPE_S64,
    SEARCH_TYPE_F32,
    SEARCH_TYPE_F64
};

const char* search_type_names[] = {
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

union value {
    uint8_t u8[8];
    uint16_t u16[4];
    uint32_t u32[2];
    uint64_t u64;
    int8_t s8[8];
    int16_t s16[4];
    int32_t s32[2];
    int64_t s64;
    float f32[2];
    double f64;
};

struct match {
    uint32_t address;
    value prev_value, curr_value;
    std::string description;
    int cpu;
    int type;
};

std::vector <match> search_matches;
std::vector <match> address_list;

int search_type = SEARCH_TYPE_U32;
int search_cmp = SEARCH_CMP_EQUAL;
int search_cpu = SEARCH_CPU_EE;
bool display_hex = false;
bool search_aligned = true;

template <typename T> bool compare_values(int cmp, T a, T b) {
    switch (cmp) {
        case SEARCH_CMP_EQUAL:
            return a == b;
        case SEARCH_CMP_NOT_EQUAL:
            return a != b;
        case SEARCH_CMP_LESS_THAN:
            return a < b;
        case SEARCH_CMP_GREATER_THAN:
            return a > b;
        case SEARCH_CMP_LESS_EQUAL:
            return a <= b;
        case SEARCH_CMP_GREATER_EQUAL:
            return a >= b;
    }

    return false;
}

void search_memory(struct ps2_state* ps2, int cpu, int type, int cmp, const char* value_str, bool aligned) {
    search_matches.clear();

    struct ps2_ram* mem = cpu == SEARCH_CPU_EE ? ps2->ee_ram : ps2->iop_ram;

    int size = 0;

    switch (type) {
        case SEARCH_TYPE_U8:
        case SEARCH_TYPE_S8:
            size = 1;
            break;
        case SEARCH_TYPE_U16:
        case SEARCH_TYPE_S16:
            size = 2;
            break;
        case SEARCH_TYPE_U32:
        case SEARCH_TYPE_S32:
        case SEARCH_TYPE_F32:
            size = 4;
            break;
        case SEARCH_TYPE_U64:
        case SEARCH_TYPE_S64:
        case SEARCH_TYPE_F64:
            size = 8;
            break;
    }

    for (int addr = 0; addr < mem->size - size; addr += (aligned ? size : 1)) {
        match m;
    
        m.address = addr;
        m.curr_value.u64 = *(uint64_t*)&mem->buf[addr];
        m.prev_value = m.curr_value;
        m.cpu = cpu;
        m.type = type;

        switch (type) {
            case SEARCH_TYPE_U8: {
                uint8_t val = (uint8_t)std::strtoul(value_str, nullptr, 0);
                if (compare_values<uint8_t>(cmp, m.curr_value.u8[0], val)) {
                    search_matches.push_back(m);
                }
            } break;
            case SEARCH_TYPE_U16: {
                uint16_t val = (uint16_t)std::strtoul(value_str, nullptr, 0);
                if (compare_values<uint16_t>(cmp, m.curr_value.u16[0], val)) {
                    search_matches.push_back(m);
                }
            } break;
            case SEARCH_TYPE_U32: {
                uint32_t val = (uint32_t)std::strtoul(value_str, nullptr, 0);
                if (compare_values<uint32_t>(cmp, m.curr_value.u32[0], val)) {
                    search_matches.push_back(m);
                }
            } break;
            case SEARCH_TYPE_U64: {
                uint64_t val = (uint64_t)std::strtoull(value_str, nullptr, 0);
                if (compare_values<uint64_t>(cmp, m.curr_value.u64, val)) {
                    search_matches.push_back(m);
                }
            } break;
            case SEARCH_TYPE_S8: {
                int8_t val = (int8_t)std::strtol(value_str, nullptr, 0);
                if (compare_values<int8_t>(cmp, m.curr_value.s8[0], val)) {
                    search_matches.push_back(m);
                }
            } break;
            case SEARCH_TYPE_S16: {
                int16_t val = (int16_t)std::strtol(value_str, nullptr, 0);
                if (compare_values<int16_t>(cmp, m.curr_value.s16[0], val)) {
                    search_matches.push_back(m);
                }
            } break;
            case SEARCH_TYPE_S32: {
                int32_t val = (int32_t)std::strtol(value_str, nullptr, 0);
                if (compare_values<int32_t>(cmp, m.curr_value.s32[0], val)) {
                    search_matches.push_back(m);
                }
            } break;
            case SEARCH_TYPE_S64: {
                int64_t val = (int64_t)std::strtoll(value_str, nullptr, 0);
                if (compare_values<int64_t>(cmp, m.curr_value.s64, val)) {
                    search_matches.push_back(m);
                }
            } break;
            case SEARCH_TYPE_F32: {
                float val = std::strtof(value_str, nullptr);
                if (compare_values<float>(cmp, m.curr_value.f32[0], val)) {
                    search_matches.push_back(m);
                }
            } break;
            case SEARCH_TYPE_F64: {
                double val = std::strtod(value_str, nullptr);
                if (compare_values<double>(cmp, m.curr_value.f64, val)) {
                    search_matches.push_back(m);
                }
            } break;
        }
    }
}

void filter_results(int type, int cmp, const char* value_str) {
    // TODO
    switch (type) {
        case SEARCH_TYPE_U8: {
            uint8_t val = (uint8_t)std::strtoul(value_str, nullptr, 0);

            std::erase_if(search_matches, [cmp, val](const match& m) {
                return !compare_values<uint8_t>(cmp, m.curr_value.u8[0], val);
            });
        } break;

        case SEARCH_TYPE_U16: {
            uint16_t val = (uint16_t)std::strtoul(value_str, nullptr, 0);

            std::erase_if(search_matches, [cmp, val](const match& m) {
                return !compare_values<uint16_t>(cmp, m.curr_value.u16[0], val);
            });
        } break;

        case SEARCH_TYPE_U32: {
            uint32_t val = (uint32_t)std::strtoul(value_str, nullptr, 0);

            std::erase_if(search_matches, [cmp, val](const match& m) {
                return !compare_values<uint32_t>(cmp, m.curr_value.u32[0], val);
            });
        } break;

        case SEARCH_TYPE_U64: {
            uint64_t val = (uint64_t)std::strtoull(value_str, nullptr, 0);

            std::erase_if(search_matches, [cmp, val](const match& m) {
                return !compare_values<uint64_t>(cmp, m.curr_value.u64, val);
            });
        } break;

        case SEARCH_TYPE_S8: {
            int8_t val = (int8_t)std::strtol(value_str, nullptr, 0);

            std::erase_if(search_matches, [cmp, val](const match& m) {
                return !compare_values<int8_t>(cmp, m.curr_value.s8[0], val);
            });
        } break;

        case SEARCH_TYPE_S16: {
            int16_t val = (int16_t)std::strtol(value_str, nullptr, 0);

            std::erase_if(search_matches, [cmp, val](const match& m) {
                return !compare_values<int16_t>(cmp, m.curr_value.s16[0], val);
            });
        } break;

        case SEARCH_TYPE_S32: {
            int32_t val = (int32_t)std::strtol(value_str, nullptr, 0);

            std::erase_if(search_matches, [cmp, val](const match& m) {
                return !compare_values<int32_t>(cmp, m.curr_value.s32[0], val);
            });
        } break;

        case SEARCH_TYPE_S64: {
            int64_t val = (int64_t)std::strtoll(value_str, nullptr, 0);

            std::erase_if(search_matches, [cmp, val](const match& m) {
                return !compare_values<int64_t>(cmp, m.curr_value.s64, val);
            });
        } break;

        case SEARCH_TYPE_F32: {
            float val = std::strtof(value_str, nullptr);

            std::erase_if(search_matches, [cmp, val](const match& m) {
                return !compare_values<float>(cmp, m.curr_value.f32[0], val);
            });
        } break;

        case SEARCH_TYPE_F64: {
            double val = std::strtod(value_str, nullptr);

            std::erase_if(search_matches, [cmp, val](const match& m) {
                return !compare_values<double>(cmp, m.curr_value.f64, val);
            });
        } break;
    }
}

void write_match_value(struct ps2_state* ps2, int cpu, match& m, int type) {
    struct ps2_ram* mem = cpu == SEARCH_CPU_EE ? ps2->ee_ram : ps2->iop_ram;

    switch (type) {
        case SEARCH_TYPE_U8:
        case SEARCH_TYPE_S8: {
            *(uint8_t*)&mem->buf[m.address] = m.curr_value.u8[0];
        } break;
        case SEARCH_TYPE_U16:
        case SEARCH_TYPE_S16: {
            *(uint16_t*)&mem->buf[m.address] = m.curr_value.u16[0];
        } break;
        case SEARCH_TYPE_U32:
        case SEARCH_TYPE_F32:
        case SEARCH_TYPE_S32: {
            *(uint32_t*)&mem->buf[m.address] = m.curr_value.u32[0];
        } break;
        case SEARCH_TYPE_U64:
        case SEARCH_TYPE_F64:
        case SEARCH_TYPE_S64: {
            *(uint64_t*)&mem->buf[m.address] = m.curr_value.u64;
        } break;
    }
}

void sprintf_match(const value& v, char* buf, size_t size, int type, int hex) {
    switch (type) {
        case SEARCH_TYPE_U8:
            snprintf(buf, size, hex ? "0x%02x" : "%u", v.u8[0]);
            break;
        case SEARCH_TYPE_U16:
            snprintf(buf, size, hex ? "0x%04x" : "%u", v.u16[0]);
            break;
        case SEARCH_TYPE_U32:
            snprintf(buf, size, hex ? "0x%08x" : "%u", v.u32[0]);
            break;
        case SEARCH_TYPE_U64:
            snprintf(buf, size, hex ? "0x%016llx" : "%llu", v.u64);
            break;
        case SEARCH_TYPE_S8:
            snprintf(buf, size, "%d", v.s8[0]);
            break;
        case SEARCH_TYPE_S16:
            snprintf(buf, size, "%d", v.s16[0]);
            break;
        case SEARCH_TYPE_S32:
            snprintf(buf, size, "%d", v.s32[0]);
            break;
        case SEARCH_TYPE_S64:
            snprintf(buf, size, "%lld", v.s64);
            break;
        case SEARCH_TYPE_F32:
            snprintf(buf, size, "%f", v.f32[0]);
            break;
        case SEARCH_TYPE_F64:
            snprintf(buf, size, "%lf", v.f64);
            break;
    }
}

void show_match_change_dialog(iris::instance* iris, match& m, char* label, int search_type, int search_cpu) {
    using namespace ImGui;

    struct ps2_state* ps2 = iris->ps2;

    static char new_value[32];

    PushFont(iris->font_small);
    TextDisabled("Edit "); SameLine(0.0, 0.0);
    PushFont(iris->font_small_code);
    Text("%s", label);
    PopFont();
    PopFont();

    PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6.0, 2.0));
    PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0, 8.0));

    PushFont(iris->font_body);
    PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.35, 0.35, 0.35, 0.35));

    AlignTextToFramePadding();
    Text(ICON_MS_EDIT); SameLine();

    SetNextItemWidth(100);
    PushFont(iris->font_code);

    if (InputText("##", new_value, 32, ImGuiInputTextFlags_EnterReturnsTrue)) {
        if (new_value[0]) {
            if (strchr(new_value, '.')) {
                if (search_type == SEARCH_TYPE_F64) {
                    m.curr_value.f64 = std::strtod(new_value, nullptr);
                } else {
                    m.curr_value.f32[0] = std::strtof(new_value, nullptr);
                }
            } else {
                m.curr_value.u64 = std::strtoull(new_value, nullptr, 0);
            }

            write_match_value(ps2, search_cpu, const_cast<match&>(m), search_type);
        }

        CloseCurrentPopup();
    }

    PopFont();

    if (Button("Change")) {
        if (new_value[0]) {
            if (strchr(new_value, '.')) {
                if (search_type == SEARCH_TYPE_F64) {
                    m.curr_value.f64 = std::strtod(new_value, nullptr);
                } else {
                    m.curr_value.f32[0] = std::strtof(new_value, nullptr);
                }
            } else {
                m.curr_value.u64 = std::strtoull(new_value, nullptr, 0);
            }

            write_match_value(ps2, search_cpu, const_cast<match&>(m), search_type);
        }

        CloseCurrentPopup();
    } SameLine();

    if (Button("Cancel"))
        CloseCurrentPopup();

    PopStyleColor();
    PopStyleVar(2);

    PopFont();
}

void show_description_change_dialog(iris::instance* iris, match& m) {
    using namespace ImGui;

    struct ps2_state* ps2 = iris->ps2;

    static char new_value[32];

    PushFont(iris->font_small);
    TextDisabled("Edit description");
    PopFont();

    PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6.0, 2.0));
    PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0, 8.0));

    PushFont(iris->font_body);
    PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.35, 0.35, 0.35, 0.35));

    AlignTextToFramePadding();
    Text(ICON_MS_EDIT); SameLine();

    SetNextItemWidth(100);

    static char desc_buf[512];

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

int frame = 0;

void show_search_table(iris::instance* iris, struct ps2_state* ps2, int type, int cpu) {
    using namespace ImGui;

    static uint32_t selected_address = 0;

    if (search_matches.empty()) {
        SeparatorText("Search results");
    } else {
        char buf[256];

        if (search_matches.size() > 9999) {
            snprintf(buf, sizeof(buf), "Search results (9999+ matches)");
        } else {
            snprintf(buf, sizeof(buf), "Search results (%zu matches)", search_matches.size());
        }

        SeparatorText(buf);
    }

    if (BeginTable("Matches", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingStretchProp)) {
        TableSetupColumn("Address");
        TableSetupColumn("Previous Value");
        TableSetupColumn("Current Value");
        TableHeadersRow();

        for (match& m : search_matches) {
            TableNextRow();

            TableSetColumnIndex(0);

            char addr_label[16];
            char prev_value_label[64];
            char curr_value_label[64];

            sprintf(addr_label, "0x%08x", m.address);
            sprintf_match(m.prev_value, prev_value_label, sizeof(prev_value_label), type, display_hex);
            sprintf_match(m.curr_value, curr_value_label, sizeof(curr_value_label), type, display_hex);

            PushFont(iris->font_code);

            Selectable(addr_label, false, ImGuiSelectableFlags_AllowOverlap | ImGuiSelectableFlags_SpanAllColumns);

            if (IsItemClicked(ImGuiMouseButton_Right)) {
                OpenPopup("context_menu");

                selected_address = m.address;
            }

            if (IsItemHovered() && IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                OpenPopup("edit_value_popup");

                selected_address = m.address;
            }

            if (selected_address == m.address) if (BeginPopup("context_menu")) {
                PushFont(iris->font_small_code);
                TextDisabled("0x%08x", m.address);
                PopFont();

                PushFont(iris->font_body);
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
                    show_match_change_dialog(iris, m, addr_label, type, cpu);

                    ImGui::EndMenu();
                }

                bool in_address_list = false;

                for (const match& am : address_list) {
                    if (am.address == m.address) {
                        in_address_list = true;
                        break;
                    }
                }

                if (Selectable(in_address_list ? ICON_MS_REMOVE " Remove from address list" : ICON_MS_ADD " Add to address list")) {
                    if (in_address_list) {
                        std::erase_if(address_list, [m](const match& am) {
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

            if (selected_address == m.address) if (BeginPopup("edit_value_popup")) {
                show_match_change_dialog(iris, m, addr_label, type, cpu);

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

void show_address_list(iris::instance* iris) {
    using namespace ImGui;

    struct ps2_state* ps2 = iris->ps2;
    static uint32_t selected_address = 0;

    if (BeginTable("Addresses", 6, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable)) {
        TableSetupColumn("Address", ImGuiTableColumnFlags_WidthFixed, 100.0f);
        TableSetupColumn("CPU", ImGuiTableColumnFlags_WidthFixed, 20.0f);
        TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 30.0f);
        TableSetupColumn("Description");
        TableSetupColumn("Previous Value", ImGuiTableColumnFlags_WidthFixed, 150.0f);
        TableSetupColumn("Current Value");
        TableHeadersRow();

        for (match& m : address_list) {
            TableNextRow();

            TableSetColumnIndex(0);

            char addr_label[16];
            char prev_value_label[64];
            char curr_value_label[64];

            sprintf(addr_label, "0x%08x", m.address);
            sprintf_match(m.prev_value, prev_value_label, sizeof(prev_value_label), m.type, display_hex);
            sprintf_match(m.curr_value, curr_value_label, sizeof(curr_value_label), m.type, display_hex);

            PushFont(iris->font_code);

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
                PushFont(iris->font_small_code);
                TextDisabled("0x%08x", m.address);
                PopFont();

                PushFont(iris->font_body);
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
                    show_match_change_dialog(iris, m, addr_label, m.type, m.cpu);

                    ImGui::EndMenu();
                }

                if (BeginMenu(ICON_MS_EDIT " Edit description")) {
                    show_description_change_dialog(iris, m);

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
                    std::erase_if(address_list, [m](const match& am) {
                        return am.address == m.address;
                    });
                }

                PopFont();

                EndPopup();
            }

            if (selected_address == m.address) if (BeginPopup("edit_value_popup_al")) {
                show_match_change_dialog(iris, m, addr_label, m.type, m.cpu);

                EndPopup();
            }

            TableSetColumnIndex(1);

            Text("%s", m.cpu == SEARCH_CPU_EE ? "EE" : "IOP");

            TableSetColumnIndex(2);

            Text("%s", search_type_names[m.type]);

            TableSetColumnIndex(3);

            PushFont(iris->font_body);
            Text("%s", m.description.c_str());
            PopFont();

            TableSetColumnIndex(4);

            Text("%s", prev_value_label);

            TableSetColumnIndex(5);

            Text("%s", curr_value_label);

            PopFont();
        }
    }

    EndTable();
}

void show_search_options(iris::instance* iris) {
    using namespace ImGui;

    struct ps2_state* ps2 = iris->ps2;

    SeparatorText("Search options");

    PushItemWidth(-FLT_MIN);

    Text("Type");
    if (BeginCombo("##type", search_type_names[search_type])) {
        for (int i = 0; i < IM_ARRAYSIZE(search_type_names); i++) {
            if (Selectable(search_type_names[i], search_type == i)) {
                search_type = i;
            }
        }

        EndCombo();
    }

    PushStyleVarY(ImGuiStyleVar_FramePadding, 2.0f);
    Checkbox("Aligned", &search_aligned);
    PopStyleVar();

    Text("Comparison");
    if (BeginCombo("##comparison", search_cmp_names[search_cmp])) {
        for (int i = 0; i < IM_ARRAYSIZE(search_cmp_names); i++) {
            if (Selectable(search_cmp_names[i], search_cmp == i)) {
                search_cmp = i;
            }
        }

        EndCombo();
    }

    Text("Memory");
    if (BeginCombo("##memory", search_cpu_names[search_cpu])) {
        for (int i = 0; i < IM_ARRAYSIZE(search_cpu_names); i++) {
            if (Selectable(search_cpu_names[i], search_cpu == i)) {
                search_cpu = i;
            }
        }

        EndCombo();
    }

    static char buf[64];

    Text("Value");
    if (InputText("##value", buf, sizeof(buf), ImGuiInputTextFlags_EnterReturnsTrue)) {
        search_memory(ps2, search_cpu, search_type, search_cmp, buf, search_aligned);
    }

    PopItemWidth();

    BeginDisabled(buf[0] == '\0');
    if (Button("Search")) {
        search_memory(ps2, search_cpu, search_type, search_cmp, buf, search_aligned);
    } SameLine();
    EndDisabled();

    BeginDisabled(buf[0] == '\0' || search_matches.empty());
    if (Button("Filter")) {
        filter_results(search_type, search_cmp, buf);
    }
    EndDisabled();
}

void update_search_matches(struct ps2_state* ps2, int cpu) {
    if (frame != 4) {
        frame++;

        return;
    }

    for (match& m : search_matches) {
        struct ps2_ram* mem = m.cpu == SEARCH_CPU_EE ? ps2->ee_ram : ps2->iop_ram;

        m.curr_value.u64 = *(uint64_t*)&mem->buf[m.address];
    }

    for (match& m : address_list) {
        struct ps2_ram* mem = m.cpu == SEARCH_CPU_EE ? ps2->ee_ram : ps2->iop_ram;

        m.curr_value.u64 = *(uint64_t*)&mem->buf[m.address];
    }


    frame = 0;
}

std::string serialize_address_list() {
    std::stringstream ss;

    for (const match& m : address_list) {
        ss << "0x" << std::hex << m.address << "," << m.description << "," << m.cpu << "," << m.type << "\n";
    }

    return ss.str();
}

void import_address_list_from_stream(std::istream& stream) {
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
            match m;

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

void show_memory_search(iris::instance* iris) {
    using namespace ImGui;

    struct ps2_state* ps2 = iris->ps2;

    update_search_matches(ps2, search_cpu);

    SetNextWindowSizeConstraints(ImVec2(600, 560), ImVec2(FLT_MAX, FLT_MAX));

    int top_shelf_height = GetContentRegionAvail().y - 220;

    if (imgui::BeginEx("Memory search", &iris->show_memory_search, ImGuiWindowFlags_MenuBar)) {
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
            show_search_table(iris, ps2, search_type, search_cpu);
        } EndChild(); SameLine();

        if (BeginChild("##search_options", ImVec2(0, GetContentRegionAvail().y - 220))) {
            show_search_options(iris);
        } EndChild();

        SeparatorText("Address list");

        if (BeginChild("##address_list")) {
            show_address_list(iris);
        } EndChild();
    } End();
}

}