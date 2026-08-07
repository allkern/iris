#pragma once

#include <cstdint>
#include <iosfwd>
#include <string>
#include <vector>

#include "applet.hpp"

namespace iris {

struct MemorySearch : Applet {
    MemorySearch() {
        id = "memory_search";
        title = "Memory search";
        flags = ImGuiWindowFlags_MenuBar;
        needs_ps2 = true;
    }

    enum {
        CMP_EQUAL,
        CMP_NOT_EQUAL,
        CMP_LESS_THAN,
        CMP_GREATER_THAN,
        CMP_LESS_EQUAL,
        CMP_GREATER_EQUAL
    };

    enum {
        CPU_EE,
        CPU_IOP
    };

    enum {
        TYPE_U8,
        TYPE_U16,
        TYPE_U32,
        TYPE_U64,
        TYPE_S8,
        TYPE_S16,
        TYPE_S32,
        TYPE_S64,
        TYPE_F32,
        TYPE_F64
    };

    union Value {
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

    struct Match {
        uint32_t address;
        Value prev_value, curr_value;
        std::string description;
        int cpu;
        int type;
    };

    bool begin() override;
    void on_render() override;

    void search(const char* value_str);
    void filter(const char* value_str);
    void update_matches();

    void show_search_table();
    void show_address_list();
    void show_search_options();
    void show_match_change_dialog(Match& m, char* label, int type, int cpu);
    void show_description_change_dialog(Match& m);

    std::string serialize_address_list();
    void import_address_list_from_stream(std::istream& stream);

    std::vector <Match> matches = {};
    std::vector <Match> address_list = {};

    int type = TYPE_U32;
    int cmp = CMP_EQUAL;
    int cpu = CPU_EE;
    bool display_hex = false;
    bool aligned = true;

    uint32_t selected_match = 0;
    uint32_t selected_address = 0;
    int update_counter = 0;

    char value_buf[64] = "";
    char edit_buf[32] = "";
    char desc_buf[512] = "";
};

}
