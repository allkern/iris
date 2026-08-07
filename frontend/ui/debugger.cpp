#include <algorithm>

#include "iris.hpp"

#include "res/IconsMaterialSymbols.h"
#include "imgui_internal.h"
#include "ee/ee_def.hpp"
#include "iop/iop_def.hpp"
#include "ps2.hpp"

namespace iris {

static bool splitter(const char* id, bool vertical, float place, float* size1, float* size2, float min1, float min2, float long_axis) {
    using namespace ImGui;

    constexpr float thickness = 1.0f;
    constexpr float grab = 4.0f;

    ImGuiWindow* window = GetCurrentWindow();

    ImRect bb;

    ImVec2 cursor = window->DC.CursorPos;
    ImVec2 size = CalcItemSize(vertical ? ImVec2(thickness, long_axis) : ImVec2(long_axis, thickness), 0.0f, 0.0f);

    bb.Min = ImVec2(cursor.x + (vertical ? place : 0.0f), cursor.y + (vertical ? 0.0f : place));
    bb.Max = ImVec2(bb.Min.x + size.x, bb.Min.y + size.y);

    PushStyleColor(ImGuiCol_SeparatorHovered, GetStyleColorVec4(ImGuiCol_Separator));
    PushStyleColor(ImGuiCol_Separator, ImVec4(0.0, 0.0, 0.0, 0.0));

    bool r = SplitterBehavior(bb, window->GetID(id), vertical ? ImGuiAxis_X : ImGuiAxis_Y, size1, size2, min1, min2, grab);

    PopStyleColor(2);

    return r;
}

static float splitter_before(bool vertical, float size1) {
    ImGuiStyle& style = ImGui::GetStyle();

    return size1 + ((vertical ? style.ItemSpacing.x : style.ItemSpacing.y) - 1.0f) * 0.5f;
}

static float splitter_at_cursor(bool vertical) {
    ImGuiStyle& style = ImGui::GetStyle();

    return -((vertical ? style.ItemSpacing.x : style.ItemSpacing.y) + 1.0f) * 0.5f;
}

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

static void show_right_column(Instance* iris, ImVec2 size, bool ee) {
    show_section(iris, "##dbg_regs", "Registers", size,
        ee ? (Applet&)iris->applets.ee_state : (Applet&)iris->applets.iop_state);
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

static float collapsed_height() {
    return 11.0f + ImGui::GetStyle().ItemSpacing.y * 2.0f + ImGui::GetStyle().WindowPadding.y * 2.0f;
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

static void show_logs_collapsible(Instance* iris, ImVec2 size, bool* open) {
    using namespace ImGui;

    if (BeginChild("##dbg_logs", size, ImGuiChildFlags_Borders)) {
        if (imgui::section(iris, "Logs", open)) {
            if (BeginTabBar("##dbg_log_tabs")) {
                show_tab("EE", "##dbg_eelog", iris->applets.ee_logs);
                show_tab("IOP", "##dbg_ioplog", iris->applets.iop_logs);
                show_tab("SYSMEM", "##dbg_syslog", iris->applets.sysmem_logs);
                show_tab("Iris", "##dbg_console", iris->applets.console);

                EndTabBar();
            }
        }
    } EndChild();
}

static void show_center_column(Instance* iris, ImVec2 size, bool ee, Debugger& d) {
    using namespace ImGui;

    if (BeginChild("##dbg_center", size)) {
        float h = GetContentRegionAvail().y;
        float spacing = GetStyle().ItemSpacing.y;
        float collapsed = collapsed_height();

        bool mem = d.show_memory;
        bool logs = d.show_logs;
        bool mem_big = mem && d.memory_open;
        bool logs_big = logs && d.logs_open;

        int gaps = (mem ? 1 : 0) + (logs ? 1 : 0);
        int expanded = (mem_big ? 1 : 0) + (logs_big ? 1 : 0);

        float fixed = 0.0f;

        if (mem && !mem_big)
            fixed += collapsed;

        if (logs && !logs_big)
            fixed += collapsed;

        float flex = std::max(160.0f, h - gaps * spacing - fixed);

        float disasm_h = flex;
        float mem_h = collapsed;
        float logs_h = collapsed;

        if (expanded > 0) {
            d.disasm_height = std::clamp(d.disasm_height, 140.0f, std::max(160.0f, flex - 100.0f * expanded));

            disasm_h = d.disasm_height;

            float below = flex - disasm_h;

            if (expanded == 2) {
                d.memory_height = std::clamp(d.memory_height, 100.0f, std::max(120.0f, below - 100.0f));

                mem_h = d.memory_height;
                logs_h = below - mem_h;
            } else if (mem_big) {
                mem_h = below;
            } else {
                logs_h = below;
            }

        }

        show_disassembly(iris, ImVec2(0, disasm_h), ee);

        float below = flex - disasm_h;

        if (mem) {
            if (mem_big) {
                float rest = below;

                splitter("##dbg_split_disasm", false, splitter_at_cursor(false), &d.disasm_height, &rest, 140.0f, 100.0f, size.x);
            }

            show_memory_panel(iris, ImVec2(0, mem_h), &d.memory_open);
        }

        if (logs) {
            if (logs_big) {
                float rest = logs_h;

                if (expanded == 2) {
                    splitter("##dbg_split_mem", false, splitter_at_cursor(false), &d.memory_height, &rest, 100.0f, 80.0f, size.x);
                } else {
                    splitter("##dbg_split_disasm", false, splitter_at_cursor(false), &d.disasm_height, &rest, 140.0f, 100.0f, size.x);
                }
            }

            show_logs_collapsible(iris, ImVec2(0, logs_h), &d.logs_open);
        }
    } EndChild();
}

static void show_layout(Instance* iris, bool ee, Debugger& d) {
    using namespace ImGui;

    ImVec2 avail = GetContentRegionAvail();

    float spacing = GetStyle().ItemSpacing.x;
    float min_center = 320.0f;

    d.left_width = std::clamp(d.left_width, 200.0f, std::max(220.0f, avail.x - min_center - 200.0f));
    d.right_width = std::clamp(d.right_width, 200.0f, std::max(220.0f, avail.x - min_center - d.left_width));

    float center_w = avail.x - d.left_width - d.right_width - spacing * 2.0f;
    float rest = avail.x - d.left_width;

    splitter("##dbg_split_left", true, splitter_before(true, d.left_width), &d.left_width, &rest, 200.0f, min_center, avail.y);

    show_left_column(iris, ImVec2(d.left_width, avail.y), ee);

    SameLine();

    splitter("##dbg_split_right", true, splitter_before(true, center_w), &center_w, &d.right_width, min_center, 200.0f, avail.y);

    show_center_column(iris, ImVec2(center_w, avail.y), ee, d);

    SameLine();

    show_right_column(iris, ImVec2(0, avail.y), ee);
}

static void show_view_menu(Instance* iris, Debugger& d) {
    using namespace ImGui;

    if (!BeginMenuBar())
        return;

    if (imgui::BeginMenu("View")) {
        imgui::MenuItem(ICON_MS_MEMORY " Memory pane", nullptr, &d.show_memory);
        imgui::MenuItem(ICON_MS_TERMINAL " Logs pane", nullptr, &d.show_logs);

        Separator();

        if (imgui::MenuItem(ICON_MS_RESTART_ALT " Reset layout")) {
            d.left_width = 300.0f;
            d.right_width = 380.0f;
            d.disasm_height = 330.0f;
            d.memory_height = 220.0f;
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
