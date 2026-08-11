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

static const char* image_format_names[] = {
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
    if (format == ImageFormat::ISIF) {
        // Only the block allocation table is written up front
        return imgui::format_size((size / 512) * sizeof(uint64_t));
    }

    return imgui::format_size(size);
}

void HddTool::on_render() {
    using namespace ImGui;

    if (BeginTabBar("##hddtooltabs")) {
        if (BeginTabItem("Create")) {
            Text("Image format");
            Combo("##image_format", &image_format, image_format_names, IM_ARRAYSIZE(image_format_names));

            Text("Size (GiB)");

            std::string size_hint = std::to_string((MIN_HDD_SIZE + (HDD_SIZE_INCREMENT * size_add)) / 0x40000000ull);

            if (BeginCombo("##sizepreset", size_hint.c_str(), 0)) {
                for (int i = 0; i < 4; i++) {
                    std::string str = std::to_string((MIN_HDD_SIZE + (HDD_SIZE_INCREMENT * i)) / 0x40000000ull);

                    if (imgui::Selectable(str.c_str())) {
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
                
                if (image_format == ImageFormat::RAW) {
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
                    if (image_format == ImageFormat::RAW) {
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
}

}