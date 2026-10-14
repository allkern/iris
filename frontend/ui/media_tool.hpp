/*
    This file is part of the Iris PlayStation 2 emulator project.
    Hosted at https://github.com/allkern/iris

    MIT License

    Copyright (c) 2026 Allkern/Lycoder (Lisandro Alarcón)

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in all
    copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
    SOFTWARE.
*/

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

    void open_for_slot(int media, int slot) {
        pending_tab = media;

        if (media == MEDIA_MEMORY_CARD) {
            mcd_slot = slot;
        } else if (media == MEDIA_USB_DRIVE) {
            usb_port = slot;
        }

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
