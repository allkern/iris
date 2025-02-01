#include <vector>
#include <string>
#include <cctype>

#include "instance.hpp"

#include "res/IconsMaterialSymbols.h"
#include "memory_viewer.h"

namespace lunar {

static MemoryEditor editor;

void show_memory_viewer(lunar::instance* lunar) {
    using namespace ImGui;

    struct ps2_state* ps2 = lunar->ps2;

    if (Begin("Memory", &lunar->show_memory_viewer)) {
        PushFont(lunar->font_code);

        editor.DrawContents(ps2->ee_ram->buf, RAM_SIZE_32MB, 0);

        PopFont();
    } End();
}

}