#include <filesystem>
#include <fstream>
#include <vector>
#include <string>
#include <cstdlib>

#include "iris.hpp"

#include "fs/mkfs.hpp"
#include "iop/usb.hpp"
#include "ps2.hpp"
#include "shared/ata/isif.hpp"

#include "res/IconsMaterialSymbols.h"
#include "portable-file-dialogs.h"

namespace iris {

static const char* const memcard_type_names[] = {
    "PS1 Memory Card",
    "PS2 Memory Card",
    "PocketStation"
};

static const char* const image_format_names[] = {
    "RAW",
    "ISIF"
};

static const char* const filesystem_names[] = {
    "FAT32",
    "exFAT"
};

static const int filesystem_types[] = {
    fs::mkfs::MKFS_FAT32,
    fs::mkfs::MKFS_EXFAT
};

static const uint64_t usb_sizes[] = {
    64ull * 1024 * 1024,
    128ull * 1024 * 1024,
    256ull * 1024 * 1024,
    512ull * 1024 * 1024,
    1024ull * 1024 * 1024,
    2048ull * 1024 * 1024,
    4096ull * 1024 * 1024,
    8192ull * 1024 * 1024,
    16384ull * 1024 * 1024,
    32768ull * 1024 * 1024
};

#define MIN_HDD_SIZE 0xa00000000ull
#define HDD_SIZE_INCREMENT 0x500000000ull

static const float item_width = 300.0f;

// pfd blocks the whole frame, so the emulator has to be silenced while it is up
static std::string ask_save_path(Instance* iris, const char* title, const std::string& default_path, const std::vector<std::string>& filters) {
    audio::mute(iris);

    auto f = pfd::save_file(title, default_path, filters);

    while (!f.ready());

    audio::unmute(iris);

    return f.result();
}

static void report(Instance* iris, int r, const char* what, const std::string& path) {
    if (r < 0) {
        push_info(iris, "Failed to create " + std::string(what) + ": " + std::string(fs::error_name(r)));
    } else {
        push_info(iris, "Created " + std::string(what) + ": \"" + path + "\"");
    }
}

static void attach_usb(Instance* iris, int port, const std::string& path) {
    iris->input.usb_devices[port] = usb::USB_DEVICE_MSD;
    iris->input.usb_msd_paths[port] = path;

    usb::set_port_device(iris->ps2->usb, port, usb::USB_DEVICE_MSD);
    usb::msd_set_image(iris->ps2->usb, port, path.c_str());
}

static void attach_memory_card(Instance* iris, int slot, const std::string& path) {
    if (!emu::attach_memory_card(iris, slot, path.c_str())) {
        push_info(iris, "Failed to attach memory card.");

        return;
    }

    if (slot == 0) {
        iris->paths.mcd0_path = path;
    } else {
        iris->paths.mcd1_path = path;
    }

    push_info(iris, "Memory card attached successfully.");
}

static void create_raw_image(const char* path, uint64_t size) {
    // Ensure file is created
    std::ofstream(path, std::ios::binary);

    // Make it big
    std::filesystem::resize_file(path, size);
}

static std::string get_file_size_string(int format, uint64_t size) {
    if (format == ImageFormat::ISIF) {
        // Only the block allocation table is written up front
        return imgui::format_size((size / 512) * sizeof(uint64_t));
    }

    return imgui::format_size(size);
}

bool MediaTool::begin() {
    ImGui::SetNextWindowSizeConstraints(ImVec2(380, 360), ImVec2(FLT_MAX, FLT_MAX));

    return Applet::begin();
}

void MediaTool::show_memory_card_tab() {
    using namespace ImGui;

    Text("Type");

    SetNextItemWidth(item_width);

    if (BeginCombo("##type", memcard_type_names[mcd_type])) {
        for (int i = 0; i < 3; i++) {
            if (imgui::Selectable(memcard_type_names[i], i == mcd_type)) {
                mcd_type = i;
            }
        }

        EndCombo();
    }

    Text("Size");

    SetNextItemWidth(item_width);

    if (mcd_type == MEMCARD_TYPE_PS2) {
        char buf[16]; sprintf(buf, "%d MB", 8 << mcd_size);

        if (BeginCombo("##size", buf)) {
            for (int i = 0; i < 5; i++) {
                sprintf(buf, "%d MB", 8 << i);

                if (imgui::Selectable(buf, i == mcd_size)) {
                    mcd_size = i;
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

    SetNextItemWidth(item_width);

    if (BeginCombo("##slot", mcd_slot == -1 ? "None" : mcd_slot == 0 ? "Slot 1" : "Slot 2")) {
        if (imgui::Selectable("None", mcd_slot == -1)) mcd_slot = -1;
        if (imgui::Selectable("Slot 1", mcd_slot == 0)) mcd_slot = 0;
        if (imgui::Selectable("Slot 2", mcd_slot == 1)) mcd_slot = 1;

        EndCombo();
    }

    PushStyleVarY(ImGuiStyleVar_FramePadding, 2.0F);
    Checkbox("Format", &mcd_format);
    PopStyleVar();

    SetItemTooltip("Lay down an empty filesystem, so the card is ready to use\nwithout formatting it from the BIOS first");

    if (Button("Create")) {
        // Data + ECC area for a PS2 card (nsects*512 + nsects*16)
        uint64_t size_in_bytes = mcd_type == MEMCARD_TYPE_PS2 ? 0x840000ull << mcd_size : 128 * 1024;

        std::string default_path = iris->paths.pref_path + (mcd_type == MEMCARD_TYPE_POCKETSTATION ? "image.psm" : "image.mcd");

        std::string path = ask_save_path(iris, "Save Memory Card image", default_path, {
            "Iris Memory Card Image (*.mcd)", "*.mcd",
            "PCSX2 Memory Card Image (*.ps2)", "*.ps2",
            "PocketStation Image (*.psm; *.pocket)", "*.psm *.pocket",
            "All Files (*.*)", "*"
        });

        if (path.size()) {
            int r;

            if (mcd_format) {
                fs::mkfs::Params params;

                params.type = mcd_type == MEMCARD_TYPE_PS2 ? fs::mkfs::MKFS_PS2_MCD : fs::mkfs::MKFS_PS1_MCD;
                params.size = size_in_bytes;

                r = fs::mkfs::format(iris->logger, path.c_str(), params);
            } else {
                // An unformatted card is erased flash, which reads back as ones
                r = fs::mkfs::create_image(iris->logger, path.c_str(), size_in_bytes, 0xff);
            }

            report(iris, r, mcd_format ? "formatted memory card image" : "blank memory card image", path);

            if (r >= 0 && mcd_slot != -1) {
                if (iris->input.mcd_slot_type[mcd_slot]) {
                    mcd_pending_path = path;

                    OpenPopup("Confirm detach");
                } else {
                    attach_memory_card(iris, mcd_slot, path);
                }
            }
        }
    }
}

void MediaTool::show_hdd_tab() {
    using namespace ImGui;

    Text("Image format");

    SetNextItemWidth(item_width);
    Combo("##image_format", &hdd_format, image_format_names, IM_ARRAYSIZE(image_format_names));

    Text("Size (GiB)");

    uint64_t size_in_bytes = MIN_HDD_SIZE + (HDD_SIZE_INCREMENT * hdd_size_add);

    SetNextItemWidth(item_width);

    if (BeginCombo("##sizepreset", std::to_string(size_in_bytes / 0x40000000ull).c_str(), 0)) {
        for (int i = 0; i < 4; i++) {
            std::string str = std::to_string((MIN_HDD_SIZE + (HDD_SIZE_INCREMENT * i)) / 0x40000000ull);

            if (imgui::Selectable(str.c_str())) {
                hdd_size_add = i;
            }
        }

        EndCombo();
    }

    if (BeginTable("##estimated-size", 2, ImGuiTableFlags_SizingFixedSame)) {
        TableNextRow();

        TableSetColumnIndex(0);
        TextDisabled("Estimated size");
        TableSetColumnIndex(1);
        Text("%s", get_file_size_string(hdd_format, size_in_bytes).c_str());

        EndTable();
    }

    PushStyleVarY(ImGuiStyleVar_FramePadding, 2.0F);
    Checkbox("Attach to PS2", &hdd_attach);
    PopStyleVar();

    if (Button("Create")) {
        std::string default_path = iris->paths.pref_path + (hdd_format == ImageFormat::RAW ? "hdd.raw" : "hdd.isif");

        std::string path = ask_save_path(iris, "Save HDD image", default_path, {
            "ISIF Image (*.isif)", "*.isif",
            "RAW Image (*.raw *.bin)", "*.raw *.bin",
            "All Files (*.*)", "*"
        });

        if (path.size()) {
            int r = 0;

            if (hdd_format == ImageFormat::RAW) {
                create_raw_image(path.c_str(), size_in_bytes);
            } else {
                r = ata::isif::create_image(iris->logger, path.c_str(), size_in_bytes / 512, 512, 1, 0, nullptr, 0);
            }

            report(iris, r, "HDD image", path);

            if (r >= 0 && hdd_attach) {
                iris->paths.hdd_path = path;
            }
        }
    }
}

void MediaTool::show_usb_drive_tab() {
    using namespace ImGui;

    Text("File system");

    SetNextItemWidth(item_width);
    Combo("##filesystem", &usb_filesystem, filesystem_names, IM_ARRAYSIZE(filesystem_names));

    Text("Size");

    SetNextItemWidth(item_width);

    if (BeginCombo("##size", imgui::format_size(usb_sizes[usb_size_index]).c_str())) {
        for (int i = 0; i < IM_ARRAYSIZE(usb_sizes); i++) {
            if (imgui::Selectable(imgui::format_size(usb_sizes[i]).c_str(), i == usb_size_index))
                usb_size_index = i;
        }

        EndCombo();
    }

    Text("Volume label");

    SetNextItemWidth(item_width);
    InputText("##label", usb_label, sizeof(usb_label));

    Text("Attach to");

    SetNextItemWidth(item_width);

    if (BeginCombo("##port", usb_port == -1 ? "None" : usb_port == 0 ? "Port 1" : "Port 2")) {
        if (imgui::Selectable("None", usb_port == -1)) usb_port = -1;
        if (imgui::Selectable("Port 1", usb_port == 0)) usb_port = 0;
        if (imgui::Selectable("Port 2", usb_port == 1)) usb_port = 1;

        EndCombo();
    }

    PushStyleVarY(ImGuiStyleVar_FramePadding, 2.0F);
    Checkbox("Partition table", &usb_partition);
    PopStyleVar();

    SetItemTooltip("Put the file system in a single MBR partition, the way a real\nthumb drive is laid out. Turn this off for a bare file system");

    if (Button("Create")) {
        std::string default_path = iris->paths.pref_path + "usb.img";

        std::string path = ask_save_path(iris, "Save USB drive image", default_path, {
            "Disk images (*.img; *.bin; *.raw)", "*.img *.bin *.raw",
            "All Files (*.*)", "*"
        });

        if (path.size()) {
            fs::mkfs::Params params;

            params.type = filesystem_types[usb_filesystem];
            params.size = usb_sizes[usb_size_index];
            params.partition = usb_partition;

            snprintf(params.label, sizeof(params.label), "%s", usb_label);

            int r = fs::mkfs::format(iris->logger, path.c_str(), params);

            report(iris, r, (std::string(filesystem_names[usb_filesystem]) + " drive image").c_str(), path);

            if (r >= 0 && usb_port != -1)
                attach_usb(iris, usb_port, path);
        }
    }
}

void MediaTool::show_confirm_detach() {
    using namespace ImGui;

    if (mcd_pending_path.empty())
        return;

    if (imgui::BeginEx("Confirm detach", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
        Text("A memory card is already attached to this slot. Do you want to detach it?");

        if (Button("Yes")) {
            attach_memory_card(iris, mcd_slot, mcd_pending_path);

            mcd_pending_path.clear();
        }

        SameLine();

        if (Button("No")) {
            mcd_pending_path.clear();
        }
    }

    End();
}

void MediaTool::on_render() {
    using namespace ImGui;

    auto tab_flags = [this](int media) {
        return pending_tab == media ? ImGuiTabItemFlags_SetSelected : 0;
    };

    if (BeginTabBar("##mediatabs")) {
        if (BeginTabItem(ICON_MS_SD_CARD " Memory card", nullptr, tab_flags(MEDIA_MEMORY_CARD))) {
            show_memory_card_tab();

            EndTabItem();
        }

        if (BeginTabItem(ICON_MS_HARD_DRIVE " HDD", nullptr, tab_flags(MEDIA_HDD))) {
            show_hdd_tab();

            EndTabItem();
        }

        if (BeginTabItem(ICON_MS_USB " USB drive", nullptr, tab_flags(MEDIA_USB_DRIVE))) {
            show_usb_drive_tab();

            EndTabItem();
        }

        if (BeginTabItem(ICON_MS_SYNC_ALT " Convert", nullptr, tab_flags(MEDIA_CONVERT))) {
            Text("This tool is a work in progress and doesn't do anything yet.");

            EndTabItem();
        }

        EndTabBar();
    }

    pending_tab = -1;

    show_confirm_detach();
}

}
