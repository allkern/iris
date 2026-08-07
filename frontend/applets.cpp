#include "iris.hpp"
#include "applets.hpp"

namespace iris {

namespace applets {

void create(Instance* iris) {
    Applets& applets = iris->applets;

    applets.all = {
        &applets.ee_control,
        &applets.ee_state,
        &applets.ee_logs,
        &applets.ee_interrupts,
        &applets.ee_dmac,
        &applets.iop_control,
        &applets.iop_state,
        &applets.iop_logs,
        &applets.iop_interrupts,
        &applets.iop_modules,
        &applets.iop_dma,
        &applets.gs_debugger,
        &applets.spu2_debugger,
        &applets.memory_viewer,
        &applets.vu_disassembler,
        &applets.breakpoints,
        &applets.about,
        &applets.compat_report,
        &applets.settings,
        &applets.pad_debugger,
        &applets.symbols,
        &applets.ee_threads,
        &applets.iop_threads,
        &applets.timers,
        &applets.sysmem_logs,
        &applets.memory_card_tool,
        &applets.memory_search,
        &applets.hdd_tool,
        &applets.gs_dump_tool,
        &applets.bios_setting
    };

    for (Applet* a : applets.all)
        a->iris = iris;
}

void init(Instance* iris) {
    for (Applet* a : iris->applets.all)
        a->on_init();
}

void render(Instance* iris) {
    for (Applet* a : iris->applets.all) {
        if (a->open && !a->was_open) {
            a->was_open = true;

            a->on_open();
        }

        if (a->open) {
            if (a->begin())
                a->on_render();

            a->end();
        }

        if (!a->open && a->was_open) {
            a->was_open = false;

            a->on_close();
        }
    }
}

}

}
