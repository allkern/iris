#pragma once

#include <string>
#include <vector>

#include "applet.hpp"
#include "fs/fs.hpp"
#include "fs/part.hpp"
#include "fs/ps1mcd.hpp"
#include "vulkan.hpp"

namespace iris {

enum : int {
    FE_DEV_MCD,
    FE_DEV_USB,
    FE_DEV_DISC,
    FE_DEV_HDD,
    FE_DEV_XFROM,
    FE_DEV_IMAGE
};

struct FileExplorerDevice {
    int kind = FE_DEV_IMAGE;
    int index = 0;
    std::string path;
    std::string label;
    std::string detail;
    bool live = false;
    bool available = false;
    bool supported = true;
};

struct FileExplorer : Applet {
    FileExplorer() {
        id = "file_explorer";
        title = "File Explorer";
        flags = ImGuiWindowFlags_MenuBar;
        needs_ps2 = true;
    }

    struct SaveIcon {
        std::string name;
        std::string title;
        Texture frames[3] = {};
        int frame_count = 0;
    };

    bool begin() override;
    void on_open() override;
    void on_render() override;
    void on_close() override;

    void open_device(const FileExplorerDevice& dev);
    void close_device();
    void mount(int partition_index);
    void navigate(const std::string& path);
    void refresh();
    void apply_filter();
    void seek(uint64_t offset);
    void load_preview();
    void free_icons();
    SaveIcon* save_icon();

    fs::blk::Device* blk = nullptr;

    iop::disc::Disc* disc = nullptr;
    fs::Fs* filesystem = nullptr;

    std::vector <fs::part::Partition> partitions;

    int partition = -1;

    void* live_udata = nullptr;

    FileExplorerDevice device;
    std::vector <FileExplorerDevice> devices;
    std::vector <FileExplorerDevice> images;

    std::string cwd = "/";

    std::vector <fs::Entry> entries;
    std::vector <int> visible;
    int selected = -1;
    char filter[128] = "";
    std::string list_error;

    std::vector <uint8_t> window;
    uint64_t window_offset = 0;
    uint64_t device_size = 0;

    std::vector <uint8_t> preview;
    std::string preview_error;
    int preview_for = -1;

    std::vector <SaveIcon> icons;

    std::string error;

    float sidebar_width = 220.0f;
    float preview_height = 220.0f;
    bool show_preview = true;
    bool hide_ecc = false;
    bool raw_view = false;
    bool show_deleted = false;
    bool show_hidden = false;
    int sort_column = 0;
    bool sort_ascending = true;
    std::string last_device;
    std::string last_extract_dir;

    bool restore = false;

    bool pending = false;
    int pending_kind = 0;
    int pending_index = 0;
};

std::string file_explorer_device_key(const FileExplorerDevice& dev);

void browse_device(Instance* iris, int kind, int index);

}
