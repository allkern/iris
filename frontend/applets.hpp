/*
    This file is part of the Iris PlayStation 2 emulator project.
    Hosted at https://github.com/allkern/iris

    MIT License

    Copyright (c) 2026 Allkern/Lycoder (Lisandro Alarcón)

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in all
    copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
    SOFTWARE.
*/

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
#include "ui/file_explorer.hpp"
#include "ui/gs.hpp"
#include "ui/gs_dump.hpp"
#include "ui/intc.hpp"
#include "ui/logs.hpp"
#include "ui/media_tool.hpp"
#include "ui/memory.hpp"
#include "ui/memory_search.hpp"
#include "ui/modules.hpp"
#include "ui/pad.hpp"
#include "ui/remote.hpp"
#include "ui/script_editor.hpp"
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
    DvdRemote dvd_remote;
    MediaTool media_tool;
    FileExplorer file_explorer;
    ScriptEditor script_editor;
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
