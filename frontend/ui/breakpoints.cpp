#include <vector>
#include <string>
#include <cctype>

#include "iris.hpp"

#include "res/IconsMaterialSymbols.h"

namespace iris {

const char* cpu_names[] = {
    "EE",
    "IOP"
};

static breakpoint* selected = nullptr;
static breakpoint editable;

void show_breakpoints_table(iris::instance* iris) {
    using namespace ImGui;

    if (BeginTable("##breakpoints", 5, ImGuiTableFlags_RowBg | ImGuiTableFlags_Sortable)) {
        PushFont(iris->font_small_code);
        TableSetupColumn("Address");
        TableSetupColumn("CPU");
        TableSetupColumn("Flags");
        TableSetupColumn("Size");
        TableSetupColumn("Actions");
        TableHeadersRow();
        PopFont();

        TableNextRow();

        int i = 0;

        for (breakpoint& b : iris->breakpoints) {
            TableSetColumnIndex(0);

            char buf[16]; sprintf(buf, "##d%x", i);

            if (Selectable(buf, &b == selected, ImGuiSelectableFlags_AllowOverlap | ImGuiSelectableFlags_SpanAllColumns)) {
                selected = &b;
            } SameLine(0.0, 0.0);

            PushFont(iris->font_code);

            if (b.symbol) {
                Text("%s", b.symbol);
            } else {
                Text("%08x", b.addr);
            }

            PopFont();

            TableSetColumnIndex(1);

            Text(b.cpu == BKPT_CPU_EE ? "EE" : "IOP");

            TableSetColumnIndex(2);

            PushFont(iris->font_code);

            Text("%c%c%c",
                b.cond_r ? 'R' : '.',
                b.cond_w ? 'W' : '.',
                b.cond_x ? 'X' : '.'
            );

            PopFont();

            TableSetColumnIndex(3);

            Text("%d", b.size);

            TableSetColumnIndex(4);

            sprintf(buf, b.enabled ? ICON_MS_CHECK "##%x" : "##%x", i);

            if (Selectable(buf, false, 0, ImVec2(20, 0))) {
                b.enabled = !b.enabled;
            } SameLine();

            sprintf(buf, ICON_MS_DELETE "##%x", i);

            if (Selectable(buf, false, 0, ImVec2(20, 0))) {
                selected = nullptr;

                iris->breakpoints.erase(iris->breakpoints.begin() + i);
            }

            i++;

            TableNextRow();
        }

        EndTable();
    }
}

uint32_t parse_address(iris::instance* iris, const char* buf, const char** name) {
    if (!buf || !buf[0])
        return 0;

    if (isalpha(buf[0]) || buf[0] == '_') {
        for (iris::elf_symbol& sym : iris->symbols) {
            if (strcmp(sym.name, buf) == 0) {
                *name = sym.name;

                return sym.addr;
            }
        }
    } else {
        *name = nullptr;

        return strtoul(buf, NULL, 16);
    }

    return 0;
}

void show_breakpoint_editor(iris::instance* iris) {
    using namespace ImGui;

    if (BeginCombo("CPU", cpu_names[editable.cpu], ImGuiComboFlags_HeightSmall)) {
        for (int i = 0; i < 2; i++) {
            if (Selectable(cpu_names[i], editable.cpu == i)) {
                editable.cpu = i;
            }
        }

        EndCombo();
    }

    static char buf[512];

    PushFont(iris->font_code);

    if (InputText("##address", buf, 512, ImGuiInputTextFlags_EnterReturnsTrue)) {
        editable.addr = parse_address(iris, buf, &editable.symbol);
    } SameLine();

    PopFont();

    Text("Address");

    Checkbox("##enabled", &editable.enabled); SameLine();
    Checkbox("##r", &editable.cond_r); SameLine();
    Checkbox("##w", &editable.cond_w); SameLine();
    Checkbox("Flags", &editable.cond_x);

    BeginDisabled(!selected);

    if (Button("Edit breakpoint")) {
        editable.addr = parse_address(iris, buf, &editable.symbol);

        *selected = editable;
    } SameLine();

    EndDisabled();

    if (Button("New breakpoint")) {
        editable.addr = parse_address(iris, buf, &editable.symbol);

        iris->breakpoints.push_back(editable);

        selected = &iris->breakpoints.back();
    }
}

void show_breakpoints(iris::instance* iris) {
    using namespace ImGui;

    if (Begin("Breakpoints", &iris->show_breakpoints, ImGuiWindowFlags_MenuBar)) {
        if (BeginMenuBar()) {
            MenuItem("Settings");

            EndMenuBar();
        }

        if (Button(ICON_MS_DELETE, ImVec2(50, 0))) {
            selected = nullptr;

            iris->breakpoints.clear();
        } SameLine();

        if (IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled | ImGuiHoveredFlags_DelayNormal)) {
            SetTooltip("Clear all");
        }

        if (Button(ICON_MS_REMOVE_SELECTION)) {
            for (breakpoint& b : iris->breakpoints) {
                b.enabled = false;
            }
        } SameLine();

        if (IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled | ImGuiHoveredFlags_DelayNormal)) {
            SetTooltip("Disable all");
        }

        if (Button(ICON_MS_SELECT)) {
            for (breakpoint& b : iris->breakpoints) {
                b.enabled = true;
            }
        }

        if (IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled | ImGuiHoveredFlags_DelayNormal)) {
            SetTooltip("Enable all");
        }

        Separator();

        if (BeginChild("##tablechild", ImVec2(0, GetContentRegionAvail().y / 2.0f))) {
            show_breakpoints_table(iris);
        } EndChild();

        SeparatorText("Add breakpoint");

        if (BeginChild("##tablechild2")) {
            show_breakpoint_editor(iris);
        } EndChild();
    } End();
}

}