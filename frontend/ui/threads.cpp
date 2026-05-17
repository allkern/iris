#include <vector>
#include <string>
#include <cctype>
#include <algorithm>
#include <regex>

#include "iris.hpp"
#include "ee/ee_def.hpp"
#include "iop/iop_def.hpp"

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

void show_ee_thread_list(iris::instance* iris) {
    using namespace ImGui;

    struct ee_state* ee = iris->ps2->ee;

    ImGuiTableFlags table_flags = ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg | ImGuiTableFlags_Sortable | ImGuiTableFlags_Hideable | ImGuiTableFlags_ScrollY;

    if (BeginTable("##threadlist_ee", 6, table_flags)) {
        TableSetupScrollFreeze(0, 1);
        TableSetupColumn("ID");
        TableSetupColumn("Priority");
        TableSetupColumn("Entry");
        TableSetupColumn("PC");
        TableSetupColumn("Argv");
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

            const char* symbol = get_entry_symbol(iris, thr->entry_point);

            if (symbol) {
                Text("%s (0x%08x)", symbol, thr->entry_point);
            } else {
                Text("0x%08X", thr->entry_point);
            }

            TableSetColumnIndex(3);
            if (thr->status == THS_RUN) {
                Text("0x%08x", ee->pc);
            } else {
                Text("0x%08x", thr->resume_addr);
            }

            uint32_t argv = *(uint32_t*)&iris->ps2->ee_ram->buf[(thr->argv + 4) & 0x1fffffff];
            TableSetColumnIndex(4);
            Text("%s", thr->argc ? (char*)&iris->ps2->ee_ram->buf[argv & 0x1fffffff] : "NULL");
            TableSetColumnIndex(5);
            Text("%s", get_status_string(thr->status));

            thr++;
        }

        EndTable();
    }
}

void show_ee_threads(iris::instance* iris) {
    using namespace ImGui;
    
    if (imgui::BeginEx("EE Threads", &iris->show_ee_threads)) {
        if (!iris->ps2->ee->thread_list_base) {
            ImVec2 size = CalcTextSize(ICON_MS_WARNING " Thread list hasn't been initialized yet");
            ImVec2 pos = ImVec2(GetContentRegionAvail().x / 2 - size.x / 2, GetContentRegionAvail().y / 2 - size.y / 2);
            ImVec4 col = GetStyle().Colors[ImGuiCol_Text];

            SetCursorPos(pos);
            TextDisabled(ICON_MS_WARNING " Thread list hasn't been initialized yet");

            End();

            return;
        }

        show_ee_thread_list(iris);
    } End();
}


void show_iop_thread_list(iris::instance* iris) {
    using namespace ImGui;

    struct iop_state* iop = iris->ps2->iop;

    ImGuiTableFlags table_flags = ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg | ImGuiTableFlags_Sortable | ImGuiTableFlags_Hideable | ImGuiTableFlags_ScrollY;

    if (BeginTable("##threadlist_iop", 5, table_flags)) {
        TableSetupScrollFreeze(0, 1);
        TableSetupColumn("ID");
        TableSetupColumn("Priority");
        TableSetupColumn("Entry");
        TableSetupColumn("PC");
        TableSetupColumn("Status");
        PushFont(iris->font_small_code);
        TableHeadersRow();
        PopFont();

        uint32_t addr = iop_read32(iop, iop->thread_list_addr);

        while (addr) {
			struct iop_thread* thr = (struct iop_thread*)&iris->ps2->iop_ram->buf[addr & 0x1fffffff];
			struct iop_thread_ctx* ctx = (struct iop_thread_ctx*)&iris->ps2->iop_ram->buf[thr->reg_storage & 0x1fffffff];

            TableNextRow();
            TableSetColumnIndex(0);
            Text("%d", thr->id);

            TableSetColumnIndex(1);
            Text("%d", thr->priority);

            TableSetColumnIndex(2);
			Text("0x%08X", thr->entry_point);

            TableSetColumnIndex(3);
            if (thr->status == THS_RUN) {
                Text("0x%08x", iop->pc);
            } else {
                Text("0x%08x", ctx->pc);
            }

            TableSetColumnIndex(4);
            Text("%s", get_status_string(thr->status));

			addr = thr->next_thread;
        }

        EndTable();
    }
}

void show_iop_threads(iris::instance* iris) {
    using namespace ImGui;
    
    if (imgui::BeginEx("IOP Threads", &iris->show_iop_threads)) {
        if (!iris->ps2->iop->thread_list_addr) {
            ImVec2 size = CalcTextSize(ICON_MS_WARNING " Thread list hasn't been initialized yet");
            ImVec2 pos = ImVec2(GetContentRegionAvail().x / 2 - size.x / 2, GetContentRegionAvail().y / 2 - size.y / 2);
            ImVec4 col = GetStyle().Colors[ImGuiCol_Text];

            SetCursorPos(pos);
            TextDisabled(ICON_MS_WARNING " Thread list hasn't been initialized yet");

            End();

            return;
        }

        show_iop_thread_list(iris);
    } End();
}

}
