#include <vector>
#include <string>
#include <cctype>

#include "iris.hpp"
#include "ee/ee_def.hpp"

#include "res/IconsMaterialSymbols.h"

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
};

std::vector <match> search_matches;

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



int frame = 0;

void show_memory_search(iris::instance* iris) {
    using namespace ImGui;

    struct ps2_state* ps2 = iris->ps2;

    static int search_type = SEARCH_TYPE_U32;
    static int search_cmp = SEARCH_CMP_EQUAL;
    static int search_cpu = SEARCH_CPU_EE;
    static bool search_aligned = true;

    if (frame == 4) {
        for (match& m : search_matches) {
            struct ps2_ram* mem = search_cpu == SEARCH_CPU_EE ? ps2->ee_ram : ps2->iop_ram;
            m.curr_value.u64 = *(uint64_t*)&mem->buf[m.address];
        }

        frame = 0;
    } else {
        frame++;
    }

    if (imgui::BeginEx("Memory Search", &iris->show_memory_search)) {
        Text("Type");
        if (BeginCombo("##type", search_type_names[search_type])) {
            for (int i = 0; i < IM_ARRAYSIZE(search_type_names); i++) {
                if (Selectable(search_type_names[i], search_type == i)) {
                    search_type = i;
                }
            }

            EndCombo();
        }

        Text("Comparison");
        if (BeginCombo("##comparison", search_cmp_names[search_cmp])) {
            for (int i = 0; i < IM_ARRAYSIZE(search_cmp_names); i++) {
                if (Selectable(search_cmp_names[i], search_cmp == i)) {
                    search_cmp = i;
                }
            }

            EndCombo();
        }
        PushStyleVarY(ImGuiStyleVar_FramePadding, 2.0f);
        Checkbox("Aligned", &search_aligned);
        PopStyleVar();

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

        if (BeginTable("Matches", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY)) {
            TableSetupColumn("Address", ImGuiTableColumnFlags_WidthFixed, 100.0f);
            TableSetupColumn("Previous Value", ImGuiTableColumnFlags_WidthFixed, 150.0f);
            TableSetupColumn("Current Value", ImGuiTableColumnFlags_WidthFixed, 150.0f);
            TableHeadersRow();

            for (match& m : search_matches) {
                TableNextRow();

                TableSetColumnIndex(0);

                char addr_label[16];
                char prev_value_label[64];
                char curr_value_label[64];

                sprintf(addr_label, "0x%08x", m.address);
                sprintf_match(m.prev_value, prev_value_label, sizeof(prev_value_label), search_type, false);
                sprintf_match(m.curr_value, curr_value_label, sizeof(curr_value_label), search_type, false);

                PushFont(iris->font_code);

                Selectable(addr_label, false, ImGuiSelectableFlags_AllowOverlap | ImGuiSelectableFlags_SpanAllColumns);

                if (IsItemClicked(ImGuiMouseButton_Right)) {
                    OpenPopup("context_menu");
                }

                if (IsItemHovered() && IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                    OpenPopup("edit_value_popup");
                }

                if (BeginPopup("context_menu")) {
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

                    if (Selectable(ICON_MS_EDIT " Edit value")) {
                        OpenPopup("edit_value_popup");
                    }

                    PopFont();

                    EndPopup();
                }

                if (BeginPopup("edit_value_popup")) {
                    static char new_value[9];

                    PushFont(iris->font_small_code);
                    TextDisabled("Edit "); SameLine(0.0, 0.0);
                    Text("%s", addr_label);
                    PopFont();

                    PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6.0, 2.0));
                    PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0, 8.0));

                    PushFont(iris->font_body);
                    PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.35, 0.35, 0.35, 0.35));

                    AlignTextToFramePadding();
                    Text(ICON_MS_EDIT); SameLine();

                    SetNextItemWidth(100);
                    PushFont(iris->font_code);

                    if (InputText("##", new_value, 9, ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_EnterReturnsTrue)) {
                        if (new_value[0]) {
                            m.curr_value.u64 = std::strtoull(new_value, nullptr, 16);

                            write_match_value(ps2, search_cpu, const_cast<match&>(m), search_type);
                        }

                        CloseCurrentPopup();
                    }

                    PopFont();

                    if (Button("Change")) {
                        if (new_value[0]) {
                            m.curr_value.u64 = std::strtoull(new_value, nullptr, 16);

                            write_match_value(ps2, search_cpu, const_cast<match&>(m), search_type);
                        }

                        CloseCurrentPopup();
                    } SameLine();

                    if (Button("Cancel"))
                        CloseCurrentPopup();

                    PopStyleColor();
                    PopStyleVar(2);

                    PopFont();
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
    } End();
}

}