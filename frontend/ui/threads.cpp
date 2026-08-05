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
        case ee::THS_RUN: return "RUN";
        case ee::THS_READY: return "READY";
        case ee::THS_WAIT: return "WAIT";
        case ee::THS_SUSPEND: return "SUSPEND";
        case ee::THS_WAITSUSPEND: return "WAIT/SUSPEND";
        case ee::THS_DORMANT: return "DORMANT";
    }

    return "<unknown>";
}

static inline const char* get_ee_wait_string(int wait) {
    switch (wait) {
        case ee::TSW_EE_NONE: return "NONE";
        case ee::TSW_EE_SLEEP: return "SLEEP";
        case ee::TSW_EE_SEMA: return "SEMA";
    }

    return "<unknown>";
}

static const char* get_entry_symbol(instance* iris, uint32_t addr) {
    // Look up the address in the symbol table
    if (addr == 0x81fc0) return "EE Idle Thread";

    for (const elf_symbol& sym : iris->symbols) {
        if ((sym.addr >= addr) && (sym.addr < (addr + sym.size))) {
            return sym.name;
        }
    }

    return nullptr;
}

void show_ee_thread_list(instance* iris) {
    using namespace ImGui;

    ee::Ee* ee = iris->ps2->ee;

    ImGuiTableFlags table_flags = ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg | ImGuiTableFlags_Sortable | ImGuiTableFlags_Hideable | ImGuiTableFlags_ScrollY;

    if (BeginTable("##threadlist_ee", 7, table_flags)) {
        TableSetupScrollFreeze(0, 1);
        TableSetupColumn("ID");
        TableSetupColumn("Priority");
        TableSetupColumn("Entry");
        TableSetupColumn("PC");
        TableSetupColumn("Argv");
        TableSetupColumn("Status");
        TableSetupColumn("WaitType");
        PushFont(iris->font_small_code);
        TableHeadersRow();
        PopFont();

        ee::Thread* thr = (ee::Thread*)&iris->ps2->ee_ram->buf[ee->thread_list_base & 0x1fffffff];

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
            if (thr->status == ee::THS_RUN) {
                Text("0x%08x", ee->pc);
            } else {
                Text("0x%08x", thr->resume_addr);
            }

            uint32_t argv = *(uint32_t*)&iris->ps2->ee_ram->buf[(thr->argv + 4) & 0x1fffffff];
            TableSetColumnIndex(4);
            Text("%s", thr->argc ? (char*)&iris->ps2->ee_ram->buf[argv & 0x1fffffff] : "NULL");
            TableSetColumnIndex(5);
            Text("%s", get_status_string(thr->status));
            TableSetColumnIndex(6);
            Text("%s", get_ee_wait_string(thr->wait_type));

            thr++;
        }

        EndTable();
    }
}

void show_ee_threads(instance* iris) {
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

static inline const char* get_iop_wait_string(int wait) {
    switch (wait) {
        case TSW_IOP_NONE: return "NONE";
        case TSW_IOP_SLEEP: return "SLEEP";
        case TSW_IOP_DELAY: return "DELAY";
        case TSW_IOP_SEMA: return "SEMA";
        case TSW_IOP_EVENTFLAG: return "EVENTFLAG";
        case TSW_IOP_MBX: return "MBX";
        case TSW_IOP_VPL: return "VPL";
        case TSW_IOP_FPL: return "FPL";
    }

    return "<unknown>";
}

void show_iop_thread_list(instance* iris) {
    using namespace ImGui;

    iop::Iop* iop = iris->ps2->iop;

    ImGuiTableFlags table_flags = ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg | ImGuiTableFlags_Sortable | ImGuiTableFlags_Hideable | ImGuiTableFlags_ScrollY;

    if (BeginTable("##threadlist_iop", 6, table_flags)) {
        TableSetupScrollFreeze(0, 1);
        TableSetupColumn("ID");
        TableSetupColumn("Priority");
        TableSetupColumn("Entry");
        TableSetupColumn("PC");
        TableSetupColumn("Status");
        TableSetupColumn("WaitType");
        PushFont(iris->font_small_code);
        TableHeadersRow();
        PopFont();

        uint32_t addr = iop::read32(iop, iop->thread_list_addr);

        while (addr) {
			iop::Thread* thr = (iop::Thread*)&iris->ps2->iop_ram->buf[addr & 0x1fffffff];
			iop::ThreadCtx* ctx = (iop::ThreadCtx*)&iris->ps2->iop_ram->buf[thr->reg_storage & 0x1fffffff];

			if (thr->tag != 0x7f01) {
				break;
			}

            TableNextRow();
            TableSetColumnIndex(0);
            Text("%d", thr->id);

            TableSetColumnIndex(1);
            Text("%d", thr->priority);

            TableSetColumnIndex(2);
			Text("0x%08X", thr->entry_point);

            TableSetColumnIndex(3);
            if (thr->status == ee::THS_RUN) {
                Text("0x%08x", iop->pc);
            } else {
                Text("0x%08x", ctx->pc);
            }

            TableSetColumnIndex(4);
            Text("%s", get_status_string(thr->status));

            TableSetColumnIndex(5);
            Text("%s", get_iop_wait_string(thr->wait_type));

			addr = thr->next_thread;
        }

        EndTable();
    }
}

void show_iop_threads(instance* iris) {
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
