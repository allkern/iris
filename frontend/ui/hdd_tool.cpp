#include <filesystem>
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cctype>
#include <cstdlib>

#include "iris.hpp"

#include "shared/ata/isif.hpp"

#include "res/IconsMaterialSymbols.h"
#include "portable-file-dialogs.h"

namespace iris {

enum : int {
    IMAGE_FMT_RAW,
    IMAGE_FMT_ISIF
};

const char* image_format_names[] = {
    "RAW",
    "ISIF"
};

#define MIN_HDD_SIZE 0xa00000000ull
#define HDD_SIZE_INCREMENT 0x500000000ull 

void create_raw_image(const char* path, uint64_t size) {
    // Ensure file is created
    std::ofstream(path, std::ios::binary);

    // Make it big
    std::filesystem::resize_file(path, size);
}

std::string get_file_size_string(int format, uint64_t size) {
    uint64_t total_size = 0;

    if (format == IMAGE_FMT_ISIF) {
        uint64_t block_count = size / 512;

        // BAT size
        total_size = block_count * sizeof(uint64_t);
    } else {
        total_size = size;
    }

    char buf[128];

    if (total_size >= 0x40000000) {
        sprintf(buf, "%.1f GiB", (float)total_size / 0x40000000ull);
    } else if (total_size >= 0x100000) {
        sprintf(buf, "%.1f MiB", (float)total_size / 0x100000ull);
    } else if (total_size >= 0x400) {
        sprintf(buf, "%.1f KiB", (float)total_size / 0x400ull);
    } else {
        sprintf(buf, "%llu B", total_size);
    }

    return std::string(buf);
}

void show_hdd_tool(Instance* iris) {
    using namespace ImGui;

    static int image_format = IMAGE_FMT_ISIF;
    static int size_add = 0;
    static bool assign = true;

    if (imgui::BeginEx("HDD Tool", &iris->ui.show_hdd_tool)) {
        if (BeginTabBar("##hddtooltabs")) {
            if (BeginTabItem("Create")) {
                Text("Image format");
                Combo("##image_format", &image_format, image_format_names, IM_ARRAYSIZE(image_format_names));

                Text("Size (GiB)");

                std::string size_hint = std::to_string((MIN_HDD_SIZE + (HDD_SIZE_INCREMENT * size_add)) / 0x40000000ull);

                if (BeginCombo("##sizepreset", size_hint.c_str(), 0)) {
                    for (int i = 0; i < 4; i++) {
                        std::string str = std::to_string((MIN_HDD_SIZE + (HDD_SIZE_INCREMENT * i)) / 0x40000000ull);

                        if (Selectable(str.c_str())) {
                            size_add = i;
                        }
                    }

                    EndCombo();
                }

                if (BeginTable("##effective-clock", 2, ImGuiTableFlags_SizingFixedSame)) {
                    TableNextRow();

                    TableSetColumnIndex(0);
                    TextDisabled("Estimated size");
                    TableSetColumnIndex(1);
                    Text("%s", get_file_size_string(image_format, MIN_HDD_SIZE + (HDD_SIZE_INCREMENT * size_add)).c_str());

                    EndTable();
                }

                PushStyleVarY(ImGuiStyleVar_FramePadding, 2.0F);
                Checkbox("Attach to PS2", &assign);
                PopStyleVar();

                if (Button("Create")) {
                    std::string default_path = iris->paths.pref_path;
                    
                    if (image_format == IMAGE_FMT_RAW) {
                        default_path += "hdd.raw";
                    } else {
                        default_path += "hdd.isif";
                    }

                    auto f = pfd::save_file("Save HDD image", default_path, {
                        "ISIF Image (*.isif)", "*.isif",
                        "RAW Image (*.raw *.bin)", "*.raw *.bin",
                        "All Files (*.*)", "*"
                    });

                    while (!f.ready());

                    if (f.result().size()) {
                        if (image_format == IMAGE_FMT_RAW) {
                            create_raw_image(f.result().c_str(), MIN_HDD_SIZE + (HDD_SIZE_INCREMENT * size_add));
                        } else {
                            uint64_t block_count = (MIN_HDD_SIZE + (HDD_SIZE_INCREMENT * size_add)) / 512;

                            ata::isif::create_image(iris->logger, f.result().c_str(), block_count, 512, 1, 0, nullptr, 0);
                        }
                    }

                    if (assign) {
                        iris->paths.hdd_path = f.result();
                    }
                }

                EndTabItem();
            }

            if (BeginTabItem("Convert")) {
                Text("This tool is a work in progress and doesn't do anything yet.");
                EndTabItem();
            }

            EndTabBar();
        }
    } End();
}

}