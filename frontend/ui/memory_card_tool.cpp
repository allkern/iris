#include <vector>
#include <string>
#include <cctype>

#include "iris.hpp"

#include "res/IconsMaterialSymbols.h"

namespace iris {

enum : int {
    MEMCARD_TYPE_PS1,
    MEMCARD_TYPE_PS2,
    MEMCARD_TYPE_POCKETSTATION
};

static const char* const type_names[] = {
    "PS1 Memory Card",
    "PS2 Memory Card",
    "PocketStation"
};

int size = 0;
int type = MEMCARD_TYPE_PS2;

void show_memory_card_tool(iris::instance* iris) {
    using namespace ImGui;

    if (Begin("Create memory card", &iris->show_memory_card_tool, ImGuiWindowFlags_NoCollapse)) {
        Text("Type");

        if (BeginCombo("##type", type_names[type])) {
            for (int i = 0; i < 3; i++) {
                if (Selectable(type_names[i], i == type)) {
                    type = i;
                }
            }

            EndCombo();
        }

        Text("Size");

        if (type == MEMCARD_TYPE_PS2) {
            char buf[16]; sprintf(buf, "%d MB", 8 << size);

            if (BeginCombo("##size", buf)) {
                for (int i = 0; i < 5; i++) {
                    sprintf(buf, "%d MB", 8 << i);

                    if (Selectable(buf, i == size)) {
                        size = i;
                    }
                }

                EndCombo();
            }
        } else {
            BeginDisabled(true);
            BeginCombo("##size", "128 KiB");
            EndDisabled();
        }

        if (Button("Create")) {
            // Create memory card file
            int size_in_bytes;

            if (type == MEMCARD_TYPE_PS2) {
                int sectors = 0x4000 << size;

                // Calculate data + ECC area
                size_in_bytes = (sectors * 512) + (sectors * 16);

                printf("size_in_bytes=%d (%x)\n", size_in_bytes, size_in_bytes);
            } else {
                size_in_bytes = 128 * 1024;

                printf("size_in_bytes=%d (%x)\n", size_in_bytes, size_in_bytes);
            }
        }
    } End();
}

}