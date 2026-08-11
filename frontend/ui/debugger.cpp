#include <algorithm>

#include "iris.hpp"

#include "res/IconsMaterialSymbols.h"
#include "imgui_internal.h"
#include "ee/ee_def.hpp"
#include "iop/iop_def.hpp"
#include "ps2.hpp"

namespace iris {

static void show_run_controls(Instance* iris) {
    using namespace ImGui;

    if (Button(iris->debug.pause ? ICON_MS_PLAY_ARROW : ICON_MS_PAUSE, ImVec2(48, 0)))
        iris->debug.pause = !iris->debug.pause;

    SameLine();

    if (Button(ICON_MS_REFRESH))
        ps2::reset(iris->ps2);

    SameLine();
    SeparatorEx(ImGuiSeparatorFlags_Vertical);
    SameLine();
}

static void show_pc(Instance* iris, uint32_t pc) {
    using namespace ImGui;

    SameLine();
    SeparatorEx(ImGuiSeparatorFlags_Vertical);
    SameLine();

    AlignTextToFramePadding();
    TextDisabled("PC");
    SameLine(0.0f, 6.0f);

    PushFont(iris->ui.font_code);
    Text("%08x", pc);
    PopFont();
}

static void show_ee_toolbar(Instance* iris) {
    using namespace ImGui;

    if (Button(ICON_MS_STEP_INTO)) {
        iris->debug.pause = true;
        iris->debug.step = true;
    }

    SameLine();

    if (Button(ICON_MS_STEP_OVER)) {
        iris->debug.step_over = true;
        iris->debug.step_over_addr = iris->ps2->ee->pc + 4;
        iris->debug.pause = false;
    }

    SameLine();

    if (Button(ICON_MS_STEP_OUT)) {
        iris->debug.step_out = true;
        iris->debug.pause = false;
    }

    SameLine();

    if (Button(ICON_MS_MOVE_DOWN))
        iris->debug.ee_control_follow_pc = true;

    SameLine();

    if (Button(ICON_MS_AUTORENEW))
        ee::flush_cache(iris->ps2->ee);

    SameLine();
    SeparatorEx(ImGuiSeparatorFlags_Vertical);
    SameLine();

    AlignTextToFramePadding();
    TextDisabled("Go to");
    SameLine();

    PushFont(iris->ui.font_code);
    SetNextItemWidth(CalcTextSize("00000000   ").x);
    InputInt("##ee_goto", (int32_t*)&iris->debug.ee_control_address, 0, 0, ImGuiInputTextFlags_CharsHexadecimal);
    PopFont();

    if (IsItemDeactivatedAfterEdit())
        iris->debug.ee_control_follow_pc = false;

    show_pc(iris, iris->ps2->ee->pc);

    if (iris->debug.symbols.empty())
        return;

    const char* func = "<unknown>";

    for (elf::Symbol& sym : iris->debug.symbols) {
        if (iris->ps2->ee->pc >= sym.addr && iris->ps2->ee->pc < (sym.addr + sym.size)) {
            func = sym.name;

            break;
        }
    }

    SameLine();
    TextDisabled("in");
    SameLine(0.0f, 6.0f);
    Text("%s", func);
}

static void show_iop_toolbar(Instance* iris) {
    using namespace ImGui;

    if (Button(ICON_MS_STEP)) {
        iris->debug.pause = true;

        ps2::step_iop(iris->ps2);
    }

    SameLine();

    if (Button(ICON_MS_MOVE_DOWN))
        iris->debug.iop_control_follow_pc = true;

    SameLine();
    SeparatorEx(ImGuiSeparatorFlags_Vertical);
    SameLine();

    AlignTextToFramePadding();
    TextDisabled("Go to");
    SameLine();

    PushFont(iris->ui.font_code);
    SetNextItemWidth(CalcTextSize("00000000   ").x);
    InputInt("##iop_goto", (int32_t*)&iris->debug.iop_control_address, 0, 0, ImGuiInputTextFlags_CharsHexadecimal);
    PopFont();

    if (IsItemDeactivatedAfterEdit())
        iris->debug.iop_control_follow_pc = false;

    show_pc(iris, iris->ps2->iop->pc);
}

static void show_tab(const char* label, const char* id, Applet& applet) {
    using namespace ImGui;

    if (!BeginTabItem(label))
        return;

    if (BeginChild(id)) {
        applet.on_render();
    } EndChild();

    EndTabItem();
}

static void show_section(Instance* iris, const char* id, const char* label, ImVec2 size, Applet& applet) {
    using namespace ImGui;

    if (BeginChild(id, size, ImGuiChildFlags_Borders)) {
        imgui::section(iris, label);

        if (BeginChild("##body")) {
            applet.on_render();
        } EndChild();
    } EndChild();
}

static void show_left_column(Instance* iris, ImVec2 size, bool ee) {
    using namespace ImGui;

    if (BeginChild("##dbg_left", size, ImGuiChildFlags_Borders)) {
        if (BeginTabBar("##dbg_left_tabs")) {
            show_tab("Breakpoints", "##dbg_bp", iris->applets.breakpoints);

            if (ee) {
                show_tab("Symbols", "##dbg_sym", iris->applets.symbols);
                show_tab("Threads", "##dbg_thr", iris->applets.ee_threads);
                show_tab("Interrupts", "##dbg_intc", iris->applets.ee_interrupts);
            } else {
                show_tab("Threads", "##dbg_thr", iris->applets.iop_threads);
                show_tab("Modules", "##dbg_mod", iris->applets.iop_modules);
                show_tab("Interrupts", "##dbg_intc", iris->applets.iop_interrupts);
            }

            EndTabBar();
        }
    } EndChild();
}

static float collapsed_height() {
    return 11.0f + ImGui::GetStyle().ItemSpacing.y * 2.0f + ImGui::GetStyle().WindowPadding.y * 2.0f;
}

static void show_disassembly(Instance* iris, ImVec2 size, bool ee) {
    using namespace ImGui;

    if (BeginChild("##dbg_disasm", size, ImGuiChildFlags_Borders)) {
        imgui::section(iris, "Disassembly");

        if (BeginChild("##body")) {
            if (ee) {
                show_ee_disassembly_view(iris);
            } else {
                show_iop_disassembly_view(iris);
            }
        } EndChild();
    } EndChild();
}

static void show_logs_collapsible(Instance* iris, ImVec2 size, Debugger& d) {
    using namespace ImGui;

    static const char* const sources[] = { "EE", "IOP", "SYSMEM", "Iris" };

    if (BeginChild("##dbg_logs", size, ImGuiChildFlags_Borders)) {
        if (imgui::section(iris, "Logs", &d.logs_open)) {
            imgui::segmented("##dbg_logsrc", &d.log_source, sources, IM_ARRAYSIZE(sources), 76.0f);

            if (d.log_source < LOG_COUNT) {
                SameLine();

                show_log_view(iris, d.log_source);
            } else {
                if (BeginChild("##dbg_console")) {
                    iris->applets.console.on_render();
                } EndChild();
            }
        }
    } EndChild();
}

static void show_memory_panel(Instance* iris, ImVec2 size, bool* open) {
    using namespace ImGui;

    if (BeginChild("##dbg_mem", size, ImGuiChildFlags_Borders)) {
        if (imgui::section(iris, "Memory", open)) {
            if (BeginChild("##body")) {
                iris->applets.memory_viewer.on_render();
            } EndChild();
        }
    } EndChild();
}

static void show_registers_panel(Instance* iris, ImVec2 size, bool ee) {
    show_section(iris, "##dbg_regs", "Registers", size,
        ee ? (Applet&)iris->applets.ee_state : (Applet&)iris->applets.iop_state);
}

static void show_right_column(Instance* iris, ImVec2 size, bool ee, Debugger& d) {
    using namespace ImGui;

    if (BeginChild("##dbg_right", size)) {
        float h = GetContentRegionAvail().y;
        float w = GetContentRegionAvail().x;
        float spacing = GetStyle().ItemSpacing.y;
        float collapsed = collapsed_height();

        if (!d.show_memory) {
            show_registers_panel(iris, ImVec2(0, 0), ee);
        } else if (!d.memory_open) {
            show_registers_panel(iris, ImVec2(0, std::max(120.0f, h - collapsed - spacing)), ee);
            show_memory_panel(iris, ImVec2(0, collapsed), &d.memory_open);
        } else {
            float flex = std::max(240.0f, h - spacing);

            d.memory_height = std::clamp(d.memory_height, 100.0f, std::max(120.0f, flex - 120.0f));

            float regs_h = flex - d.memory_height;

            show_registers_panel(iris, ImVec2(0, regs_h), ee);

            imgui::splitter("##dbg_split_mem", false, imgui::splitter_at_cursor(false), &regs_h, &d.memory_height, 120.0f, 100.0f, w);

            show_memory_panel(iris, ImVec2(0, d.memory_height), &d.memory_open);
        }
    } EndChild();
}

static void show_center_column(Instance* iris, ImVec2 size, bool ee, Debugger& d) {
    using namespace ImGui;

    if (BeginChild("##dbg_center", size)) {
        float h = GetContentRegionAvail().y;
        float w = GetContentRegionAvail().x;
        float spacing = GetStyle().ItemSpacing.y;
        float collapsed = collapsed_height();

        if (!d.show_logs) {
            show_disassembly(iris, ImVec2(0, 0), ee);
        } else if (!d.logs_open) {
            show_disassembly(iris, ImVec2(0, std::max(160.0f, h - collapsed - spacing)), ee);
            show_logs_collapsible(iris, ImVec2(0, collapsed), d);
        } else {
            float flex = std::max(240.0f, h - spacing);

            d.disasm_height = std::clamp(d.disasm_height, 140.0f, std::max(160.0f, flex - 100.0f));

            float logs_h = flex - d.disasm_height;

            show_disassembly(iris, ImVec2(0, d.disasm_height), ee);

            imgui::splitter("##dbg_split_disasm", false, imgui::splitter_at_cursor(false), &d.disasm_height, &logs_h, 140.0f, 100.0f, w);

            show_logs_collapsible(iris, ImVec2(0, logs_h), d);
        }
    } EndChild();
}

static void show_layout(Instance* iris, bool ee, Debugger& d) {
    using namespace ImGui;

    ImVec2 avail = GetContentRegionAvail();

    float spacing = GetStyle().ItemSpacing.x;
    float min_center = 320.0f;

    float left_w = d.show_left ? d.left_width : 0.0f;

    if (d.show_left) {
        d.left_width = std::clamp(d.left_width, 200.0f, std::max(220.0f, avail.x - min_center - 200.0f));

        left_w = d.left_width;
    }

    d.right_width = std::clamp(d.right_width, 200.0f, std::max(220.0f, avail.x - min_center - left_w));

    float center_w = avail.x - left_w - d.right_width - spacing * (d.show_left ? 2.0f : 1.0f);

    if (d.show_left) {
        float rest = avail.x - d.left_width;

        imgui::splitter("##dbg_split_left", true, imgui::splitter_before(true, d.left_width), &d.left_width, &rest, 200.0f, min_center, avail.y);

        show_left_column(iris, ImVec2(d.left_width, avail.y), ee);

        SameLine();
    }

    imgui::splitter("##dbg_split_right", true, imgui::splitter_before(true, center_w), &center_w, &d.right_width, min_center, 200.0f, avail.y);

    show_center_column(iris, ImVec2(center_w, avail.y), ee, d);

    SameLine();

    show_right_column(iris, ImVec2(0, avail.y), ee, d);
}

static void show_view_menu(Instance* iris, Debugger& d) {
    using namespace ImGui;

    if (!BeginMenuBar())
        return;

    if (imgui::BeginMenu("View")) {
        imgui::MenuItem(ICON_MS_VIEW_SIDEBAR " Left panel", nullptr, &d.show_left);
        imgui::MenuItem(ICON_MS_MEMORY " Memory pane", nullptr, &d.show_memory);
        imgui::MenuItem(ICON_MS_TERMINAL " Logs pane", nullptr, &d.show_logs);

        Separator();

        if (imgui::MenuItem(ICON_MS_RESTART_ALT " Reset layout")) {
            d.left_width = 300.0f;
            d.right_width = 380.0f;
            d.disasm_height = 330.0f;
            d.memory_height = 220.0f;
            d.show_left = true;
            d.show_memory = true;
            d.show_logs = true;
            d.memory_open = true;
            d.logs_open = true;
        }

        ImGui::EndMenu();
    }

    EndMenuBar();
}

bool Debugger::begin() {
    ImGui::SetNextWindowSizeConstraints(ImVec2(1040, 600), ImVec2(FLT_MAX, FLT_MAX));
    ImGui::PushFont(iris->ui.font_icons);

    return Applet::begin();
}

void Debugger::end() {
    ImGui::End();
    ImGui::PopFont();
}

void Debugger::on_render() {
    using namespace ImGui;

    static const char* const cpu_names[] = { "EE", "IOP" };

    show_view_menu(iris, *this);

    imgui::segmented("##dbg_cpu", &cpu, cpu_names, 2, 52.0f);

    SameLine();
    SeparatorEx(ImGuiSeparatorFlags_Vertical);
    SameLine();

    show_run_controls(iris);

    bool ee = cpu == 0;

    if (ee) {
        show_ee_toolbar(iris);
    } else {
        show_iop_toolbar(iris);
    }

    show_layout(iris, ee, *this);
}

}
