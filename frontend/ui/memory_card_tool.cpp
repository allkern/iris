#include <vector>
#include <string>
#include <cctype>
#include <cstdlib>

#include "iris.hpp"

#include "res/IconsMaterialSymbols.h"
#include "portable-file-dialogs.h"

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

    if (imgui::BeginEx("Create memory card", &iris->show_memory_card_tool, ImGuiWindowFlags_NoCollapse)) {
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
                // Calculate data + ECC area (nsects*512 + nsects*16)
                size_in_bytes = 0x840000 << size;
            } else {
                size_in_bytes = 128 * 1024;
            }

            audio::mute(iris);

            std::string default_path = iris->pref_path + "image.mcd";

            if (type == MEMCARD_TYPE_POCKETSTATION) {
                default_path = iris->pref_path + "image.psm";
            }

            auto f = pfd::save_file("Save Memory Card image", default_path, {
                "Iris Memory Card Image (*.mcd)", "*.mcd",
                "PCSX2 Memory Card Image (*.ps2)", "*.ps2",
                "PocketStation Image (*.psm; *.pocket)", "*.psm *.pocket",
                "All Files (*.*)", "*"
            });

            while (!f.ready());

            audio::unmute(iris);

            if (f.result().size()) {
                FILE* file = fopen(f.result().c_str(), "wb");

                void* buf = malloc(size_in_bytes);

                memset(buf, 0, size_in_bytes);

                fseek(file, 0, SEEK_SET);
                fwrite(buf, size_in_bytes, 1, file);
                fclose(file);

                char msg[1024];

                sprintf(msg, "Created memory card image: \"%s\"", f.result().c_str());

                push_info(iris, std::string(msg));

                free(buf);
            }
        }
    } End();
}

}