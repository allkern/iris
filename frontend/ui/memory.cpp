#include <vector>
#include <string>
#include <cctype>

#include "iris.hpp"

#include "res/IconsMaterialSymbols.h"
#include "memory_viewer.h"

namespace iris {

static MemoryEditor editor;

void show_memory_viewer(iris::instance* iris) {
    using namespace ImGui;

    struct ps2_state* ps2 = iris->ps2;

    if (Begin("Memory", &iris->show_memory_viewer)) {
        PushFont(iris->font_code);

        editor.DrawContents(ps2->ee_ram->buf, RAM_SIZE_32MB, 0);

        PopFont();
    } End();
}

}