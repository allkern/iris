#include <vector>
#include <string>
#include <cctype>
#include <algorithm>
#include <regex>

#include "iris.hpp"
#include "ee/ee_def.hpp"

#include "res/IconsMaterialSymbols.h"

namespace iris {

static inline const char* get_status_string(int status) {
    switch (status) {
        case THS_RUN: return "RUN";
        case THS_READY: return "READY";
        case THS_WAIT: return "WAIT";
        case THS_SUSPEND: return "SUSPEND";
        case THS_WAITSUSPEND: return "WAIT/SUSPEND";
        case THS_DORMANT: return "DORMANT";
    }

    return "<unknown>";
}

static const char* get_entry_symbol(iris::instance* iris, uint32_t addr) {
    // Look up the address in the symbol table
    if (addr == 0x81fc0) return "EE Idle Thread";

    for (const iris::elf_symbol& sym : iris->symbols) {
        if ((sym.addr >= addr) && (sym.addr < (addr + sym.size))) {
            return sym.name;
        }
    }

    return nullptr;
}

void show_thread_list(iris::instance* iris) {
    using namespace ImGui;

    struct ee_state* ee = iris->ps2->ee;

    ImGuiTableFlags table_flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_Sortable | ImGuiTableFlags_Hideable | ImGuiTableFlags_ScrollY;

    if (BeginTable("##threadlist", 5, table_flags)) {
        TableSetupScrollFreeze(0, 1);
        TableSetupColumn("ID");
        TableSetupColumn("Priority");
        TableSetupColumn("Entry");
        TableSetupColumn("PC");
        TableSetupColumn("Status");
        PushFont(iris->font_small_code);
        TableHeadersRow();
        PopFont();

        struct ee_thread* thr = (struct ee_thread*)&iris->ps2->ee_ram->buf[ee->thread_list_base & 0x1fffffff];

        int id = 0;

        while (thr->status) {
            TableNextRow();
            TableSetColumnIndex(0);
            Text("%d", id++);
            TableSetColumnIndex(1);
            Text("%d", thr->current_priority);
            TableSetColumnIndex(2);

            const char* symbol = get_entry_symbol(iris, thr->func);

            if (symbol) {
                Text("%s", symbol);
            } else {
                Text("0x%08X", thr->func);
            }

            uint32_t argv = *(uint32_t*)&iris->ps2->ee_ram->buf[(thr->argv + 4) & 0x1fffffff];

            // if (thr->argc) {
            //     printf("argv=%08x *argv=%08x\n", thr->argv, argv);
            // }

            TableSetColumnIndex(3);
            Text("%s", thr->argc ? (char*)&iris->ps2->ee_ram->buf[argv & 0x1fffffff] : "NULL");
            TableSetColumnIndex(4);
            Text("%s", get_status_string(thr->status));

            thr++;
        }

        EndTable();
    }
}

void show_threads(iris::instance* iris) {
    using namespace ImGui;
    
    if (imgui::BeginEx("Threads", &iris->show_threads)) {
        if (!iris->ps2->ee->thread_list_base) {
            ImVec2 size = CalcTextSize(ICON_MS_WARNING " Thread list hasn't been initialized yet");
            ImVec2 pos = ImVec2(GetContentRegionAvail().x / 2 - size.x / 2, GetContentRegionAvail().y / 2 - size.y / 2);
            ImVec4 col = GetStyle().Colors[ImGuiCol_Text];

            SetCursorPos(pos);
            TextDisabled(ICON_MS_WARNING " Thread list hasn't been initialized yet");

            End();

            return;
        }

        show_thread_list(iris);
    } End();
}

}