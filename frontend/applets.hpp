#pragma once

#include <vector>

#include "applet.hpp"

#include "ui/about.hpp"
#include "ui/bios_setting.hpp"
#include "ui/breakpoints.hpp"
#include "ui/compat_report.hpp"
#include "ui/console.hpp"
#include "ui/control.hpp"
#include "ui/debugger.hpp"
#include "ui/dma.hpp"
#include "ui/gs.hpp"
#include "ui/gs_dump.hpp"
#include "ui/hdd_tool.hpp"
#include "ui/intc.hpp"
#include "ui/logs.hpp"
#include "ui/memory.hpp"
#include "ui/memory_card_tool.hpp"
#include "ui/memory_search.hpp"
#include "ui/modules.hpp"
#include "ui/pad.hpp"
#include "ui/settings.hpp"
#include "ui/spu2.hpp"
#include "ui/state.hpp"
#include "ui/symbols.hpp"
#include "ui/threads.hpp"
#include "ui/timers.hpp"
#include "ui/vu_disassembly.hpp"

namespace iris {

struct Applets {
    EeControl ee_control;
    EeState ee_state;
    EeInterrupts ee_interrupts;
    EeDmac ee_dmac;
    EeThreads ee_threads;
    IopControl iop_control;
    IopState iop_state;
    IopInterrupts iop_interrupts;
    IopModules iop_modules;
    IopDma iop_dma;
    IopThreads iop_threads;
    Logs logs;
    Console console;
    Debugger debugger;
    GsDebugger gs_debugger;
    Spu2Debugger spu2_debugger;
    MemoryViewer memory_viewer;
    MemorySearch memory_search;
    VuDisassembler vu_disassembler;
    Breakpoints breakpoints;
    Symbols symbols;
    Timers timers;
    PadDebugger pad_debugger;
    MemoryCardTool memory_card_tool;
    HddTool hdd_tool;
    GsDumpTool gs_dump_tool;
    BiosSetting bios_setting;
    SettingsWindow settings;
    CompatReport compat_report;
    About about;

    std::vector <Applet*> all = {};
};

namespace applets {

void create(Instance* iris);
void init(Instance* iris);
void render(Instance* iris);

}

}
