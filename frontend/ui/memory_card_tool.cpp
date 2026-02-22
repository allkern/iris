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
int slot = 0;
const char* fpath = nullptr;

void show_memory_card_tool(iris::instance* iris) {
    using namespace ImGui;

    SetNextWindowSizeConstraints(ImVec2(350, 320), ImVec2(FLT_MAX, FLT_MAX));

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

        Text("Attach to");

        if (BeginCombo("##slot", slot == -1 ? "None" : slot == 0 ? "Slot 1" : "Slot 2")) {
            if (Selectable("None", slot == -1)) {
                slot = -1;
            }

            if (Selectable("Slot 1", slot == 0)) {
                slot = 0;
            }

            if (Selectable("Slot 2", slot == 1)) {
                slot = 1;
            }

            EndCombo();
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

                const int shift = 4;

                void* buf = malloc(size_in_bytes >> shift);

                memset(buf, 0xff, size_in_bytes >> shift);

                fseek(file, 0, SEEK_SET);

                for (int i = 0; i < 1 << shift; i++) {
                    fwrite(buf, size_in_bytes >> shift, 1, file);
                }

                fclose(file);

                char msg[1024];

                sprintf(msg, "Created memory card image: \"%s\"", f.result().c_str());

                push_info(iris, std::string(msg));

                free(buf);

                if (slot != -1) {
                    if (iris->mcd_slot_type[slot]) {
                        fpath = f.result().c_str();

                        OpenPopup("Confirm detach");
                    } else {
                        // Attach memory card to slot
                        if (emu::attach_memory_card(iris, slot, f.result().c_str())) {
                            push_info(iris, "Memory card attached successfully.");

                            if (slot == 0) {
                                iris->mcd0_path = f.result();
                            } else {
                                iris->mcd1_path = f.result();
                            }
                        } else {
                            push_info(iris, "Failed to attach memory card.");
                        }
                    }
                }
            }
        }

        if (fpath && imgui::BeginEx("Confirm detach", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
            Text("A memory card is already attached to this slot. Do you want to detach it?");

            if (Button("Yes")) {
                if (emu::attach_memory_card(iris, slot, fpath)) {
                    if (slot == 0) {
                        iris->mcd0_path = std::string(fpath);
                    } else {
                        iris->mcd1_path = std::string(fpath);
                    }

                    push_info(iris, "Memory card attached successfully.");
                } else {
                    push_info(iris, "Failed to attach memory card.");
                }

                fpath = nullptr;
            }

            SameLine();

            if (Button("No")) {
                fpath = nullptr;
            }

            End();
        }
    } End();
}

}