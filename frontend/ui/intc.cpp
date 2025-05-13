#include <vector>
#include <string>
#include <cctype>

#include "iris.hpp"

#include "res/IconsMaterialSymbols.h"

namespace iris {

const char* ee_irq_sources[] = {
    "GS",
    "SBUS",
    "Vblank In",
    "Vblank Out",
    "VIF0",
    "VIF1",
    "VU0",
    "VU1",
    "IPU",
    "Timer 0",
    "Timer 1",
    "Timer 2",
    "Timer 3",
    "SFIFO",
    "VU0 Watchdog"
};

const char* iop_irq_sources[] = {
    "Vblank In",
    "GPU",
    "CDVD",
    "DMA",
    "Timer 0",
    "Timer 1",
    "Timer 2",
    "SIO0",
    "SIO1",
    "SPU2",
    "PIO",
    "Vblank Out",
    "DVD",
    "PCMCIA",
    "Timer 3",
    "Timer 4",
    "Timer 5",
    "SIO2",
    "HTR0",
    "HTR1",
    "HTR2",
    "HTR3",
    "USB",
    "EXTR",
    "FWRE",
    "FDMA"
};

void show_ee_intc_interrupts(iris::instance* iris) {
    using namespace ImGui;

    struct ps2_intc* intc = iris->ps2->ee_intc;

    if (BeginTable("##eeintc", 3, ImGuiTableFlags_RowBg)) {
        PushFont(iris->font_small_code);
        TableSetupColumn("Source");
        TableSetupColumn("Status");
        TableSetupColumn("Mask");
        TableHeadersRow();
        PopFont();

        for (int i = 0; i < 15; i++) {
            TableNextRow();

            TableSetColumnIndex(0);

            Text("%s", ee_irq_sources[i]);

            TableSetColumnIndex(1);

            int status = intc->stat & (1 << i);
            int mask = intc->mask & (1 << i);

            char label[16];

            sprintf(label, "%s##s%x", status ? ICON_MS_CHECK : "", i);

            if (Selectable(label)) {
                intc->stat ^= 1 << i;
            }

            TableSetColumnIndex(2);

            sprintf(label, "%s##m%x", mask ? ICON_MS_CHECK : "", i);

            if (Selectable(label)) {
                intc->stat ^= 1 << i;
            }
        }

        EndTable();
    }
}

void show_ee_interrupts(iris::instance* iris) {
    using namespace ImGui;

    struct ps2_intc* intc = iris->ps2->ee_intc;

    if (Begin("EE Interrupts", &iris->show_ee_interrupts)) {
        if (Button(ICON_MS_REMOVE_SELECTION)) {
            intc->mask = 0;
        } SameLine();

        if (IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled | ImGuiHoveredFlags_DelayNormal)) {
            SetTooltip("Disable all");
        }

        if (Button(ICON_MS_SELECT)) {
            intc->mask |= 0xffff;
        }

        if (IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled | ImGuiHoveredFlags_DelayNormal)) {
            SetTooltip("Enable all");
        }

        if (BeginChild("##eeintcchild")) {
            show_ee_intc_interrupts(iris);
        } EndChild();
    } End();
}

void show_iop_intc_interrupts(iris::instance* iris) {
    using namespace ImGui;

    struct ps2_iop_intc* intc = iris->ps2->iop_intc;

    if (BeginTable("##iopintc", 3, ImGuiTableFlags_RowBg)) {
        PushFont(iris->font_small_code);
        TableSetupColumn("Source");
        TableSetupColumn("Status");
        TableSetupColumn("Mask");
        TableHeadersRow();
        PopFont();

        for (int i = 0; i < 26; i++) {
            TableNextRow();

            TableSetColumnIndex(0);

            Text("%s", iop_irq_sources[i]);

            TableSetColumnIndex(1);

            int status = intc->stat & (1 << i);
            int mask = intc->mask & (1 << i);

            char label[16];

            sprintf(label, "%s##s%x", status ? ICON_MS_CHECK : "", i);

            if (Selectable(label)) {
                intc->stat ^= 1 << i;
            }

            TableSetColumnIndex(2);

            sprintf(label, "%s##m%x", mask ? ICON_MS_CHECK : "", i);

            if (Selectable(label)) {
                intc->mask ^= 1 << i;
            }
        }

        EndTable();
    }
}

void show_iop_interrupts(iris::instance* iris) {
    using namespace ImGui;

    struct ps2_iop_intc* intc = iris->ps2->iop_intc;

    if (Begin("IOP Interrupts", &iris->show_iop_interrupts)) {
        if (Button(ICON_MS_REMOVE_SELECTION)) {
            intc->mask = 0;
        } SameLine();

        if (IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled | ImGuiHoveredFlags_DelayNormal)) {
            SetTooltip("Disable all");
        }

        if (Button(ICON_MS_SELECT)) {
            intc->mask |= 0xffff;
        }

        if (IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled | ImGuiHoveredFlags_DelayNormal)) {
            SetTooltip("Enable all");
        }

        if (BeginChild("##iopintcchild")) {
            show_iop_intc_interrupts(iris);
        } EndChild();
    } End();
}

}