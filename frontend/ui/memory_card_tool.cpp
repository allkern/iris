#include <vector>
#include <string>
#include <cctype>

#include "iris.hpp"

#include "res/IconsMaterialSymbols.h"

namespace iris {

int size = 0;

void show_memory_card_tool(iris::instance* iris) {
    using namespace ImGui;

    if (Begin("Create memory card", &iris->show_memory_card_tool, ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoCollapse)) {
        if (BeginMenuBar()) {
            MenuItem("File");
            MenuItem("Edit");

            EndMenuBar();
        }

        Text("Size");

        char buf[16]; sprintf(buf, "%d MB", 8 << size);

        if (BeginCombo("##size", buf)) {
            for (int i = 0; i < 4; i++) {
                sprintf(buf, "%d MB", 8 << i);

                if (Selectable(buf, i == size)) {
                    size = i;
                }
            }

            EndCombo();
        }
    } End();
}

}