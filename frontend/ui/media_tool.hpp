#pragma once

#include <string>

#include "applet.hpp"

namespace iris {

enum ImageFormat : int {
    RAW,
    ISIF
};

enum : int {
    MEDIA_MEMORY_CARD,
    MEDIA_HDD,
    MEDIA_USB_DRIVE,
    MEDIA_CONVERT
};

enum : int {
    MEMCARD_TYPE_PS1,
    MEMCARD_TYPE_PS2,
    MEMCARD_TYPE_POCKETSTATION
};

struct MediaTool : Applet {
    MediaTool() {
        id = "media_tool";
        title = "Create media image";
        flags = ImGuiWindowFlags_NoCollapse;
        persist = false;
    }

    bool begin() override;
    void on_render() override;

    void show_memory_card_tab();
    void show_hdd_tab();
    void show_usb_drive_tab();
    void show_confirm_detach();

    // Opens the applet on the tab for a specific kind of media
    void open_for(int media) {
        pending_tab = media;

        show();
    }

    int pending_tab = -1;

    int mcd_type = MEMCARD_TYPE_PS2;
    int mcd_size = 0;
    int mcd_slot = 0;
    bool mcd_format = true;
    std::string mcd_pending_path;

    int hdd_format = ImageFormat::ISIF;
    int hdd_size_add = 0;
    bool hdd_attach = true;

    int usb_filesystem = 0;
    int usb_size_index = 0;
    int usb_port = 0;
    bool usb_partition = true;
    char usb_label[12] = "IRIS";
};

}
