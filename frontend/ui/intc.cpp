#include <vector>
#include <string>
#include <cctype>

#include "iris.hpp"

#include "res/IconsMaterialSymbols.h"
#include "ps2.hpp"

namespace iris {

static const char* ee_irq_sources[] = {
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

static const char* ee_dmac_irq_sources[] = {
    "VIF0",
    "VIF1",
    "GIF",
    "IPU_FROM",
    "IPU_TO",
    "SIF0",
    "SIF1",
    "SIF2",
    "SPR_FROM",
    "SPR_TO"
};

static const char* iop_irq_sources[] = {
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

void show_ee_intc_interrupts(Instance* iris) {
    using namespace ImGui;

    ee::intc::Intc* intc = iris->ps2->ee_intc;

    if (BeginTable("##eeintc", 3, ImGuiTableFlags_RowBg)) {
        PushFont(iris->ui.font_small_code);
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
                intc->mask ^= 1 << i;
            }
        }

        EndTable();
    }
}

void show_ee_dmac_interrupts(Instance* iris) {
    using namespace ImGui;

    ee::dmac::Dmac* dmac = iris->ps2->ee_dma;

    if (BeginTable("##eedmac", 3, ImGuiTableFlags_RowBg)) {
        PushFont(iris->ui.font_small_code);
        TableSetupColumn("Source");
        TableSetupColumn("Status");
        TableSetupColumn("Mask");
        TableHeadersRow();
        PopFont();

        for (int i = 0; i < 10; i++) {
            TableNextRow();

            TableSetColumnIndex(0);

            Text("%s", ee_dmac_irq_sources[i]);

            TableSetColumnIndex(1);

            int status = dmac->stat & (1 << i);
            int mask = dmac->stat & (1 << (i + 16));

            char label[16];

            sprintf(label, "%s##ds%x", status ? ICON_MS_CHECK : "", i);

            if (Selectable(label)) {
                dmac->stat ^= 1 << i;
            }

            TableSetColumnIndex(2);

            sprintf(label, "%s##dm%x", mask ? ICON_MS_CHECK : "", i);

            if (Selectable(label)) {
                dmac->stat ^= 1 << (i + 16);
            }
        }

        EndTable();
    }
}

void EeInterrupts::on_render() {
    using namespace ImGui;

    ee::intc::Intc* intc = iris->ps2->ee_intc;

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
        if (TreeNode("INTC")) {
            show_ee_intc_interrupts(iris);

            TreePop();
        }

        if (TreeNode("DMAC")) {
            show_ee_dmac_interrupts(iris);

            TreePop();
        }
    } EndChild();
}

void show_iop_intc_interrupts(Instance* iris) {
    using namespace ImGui;

    iop::intc::Intc* intc = iris->ps2->iop_intc;

    if (BeginTable("##iopintc", 3, ImGuiTableFlags_RowBg)) {
        PushFont(iris->ui.font_small_code);
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

void IopInterrupts::on_render() {
    using namespace ImGui;

    iop::intc::Intc* intc = iris->ps2->iop_intc;

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
}

}