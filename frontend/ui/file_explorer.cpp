#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>

#include "iris.hpp"

#include "res/IconsMaterialSymbols.h"
#include "imgui_memory_editor.h"
#include "imgui_internal.h"
#include "portable-file-dialogs.h"

#include "dev/mcd.hpp"
#include "dev/ps1_mcd.hpp"
#include "iop/sio2.hpp"
#include "iop/disc.hpp"
#include "fs/pfs.hpp"
#include "shared/speed.hpp"
#include "shared/speed/ata.hpp"
#include "iop/usb.hpp"
#include "iop/cdvd.hpp"
#include "ps2.hpp"

namespace iris {

static constexpr uint64_t WINDOW_SIZE = 0x10000;
static constexpr uint64_t PREVIEW_LIMIT = 0x100000;
static constexpr int MAX_EXTRACT_DEPTH = 64;

struct Action {
    std::string go;
    std::string extract;
    bool directory = false;
};

static MemoryEditor editor;

std::string file_explorer_device_key(const FileExplorerDevice& dev) {
    switch (dev.kind) {
        case FE_DEV_MCD: return "mcd" + std::to_string(dev.index);
        case FE_DEV_USB: return "usb" + std::to_string(dev.index);
        case FE_DEV_DISC: return "disc";
        case FE_DEV_HDD: return "hdd";
        case FE_DEV_XFROM: return "xfrom";
    }

    return "image:" + dev.path;
}

static void* mcd_udata(Instance* iris, int slot) {
    if (!iris->ps2 || !iris->ps2->sio2)
        return nullptr;

    return iris->ps2->sio2->port[slot + 2].udata;
}

static iop::disc::Disc* live_disc(Instance* iris) {
    if (!iris->ps2 || !iris->ps2->cdvd)
        return nullptr;

    return iris->ps2->cdvd->disc;
}

static speed::flash::Flash* live_flash(Instance* iris) {
    if (!iris->ps2 || !iris->ps2->speed || !iris->ps2->speed->flash)
        return nullptr;

    return iris->ps2->speed->flash->id ? iris->ps2->speed->flash : nullptr;
}

static bool looks_like_disc(const std::string& path) {
    size_t dot = path.find_last_of('.');

    if (dot == std::string::npos)
        return false;

    std::string ext = path.substr(dot + 1);

    for (char& c : ext) {
        if (c >= 'A' && c <= 'Z')
            c += 32;
    }

    return ext == "iso" || ext == "cue" || ext == "chd" || ext == "cso" || ext == "zso";
}

static const char* mcd_type_name(int type) {
    switch (type) {
        case 1: return "PS2 memory card";
        case 2: return "PS1 memory card";
        case 3: return "PocketStation";
    }

    return "Memory card";
}

static std::string format_time(const fs::Time& t) {
    if (!t.valid)
        return "";

    char buf[32];

    snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d", t.year, t.month, t.day, t.hour, t.minute);

    return buf;
}

static int compare_time(const fs::Time& a, const fs::Time& b) {
    if (a.valid != b.valid)
        return a.valid ? 1 : -1;

    int lhs[6] = { a.year, a.month, a.day, a.hour, a.minute, a.second };
    int rhs[6] = { b.year, b.month, b.day, b.hour, b.minute, b.second };

    for (int i = 0; i < 6; i++) {
        if (lhs[i] != rhs[i])
            return lhs[i] < rhs[i] ? -1 : 1;
    }

    return 0;
}

static std::string format_flags(uint32_t flags) {
    std::string s;

    if (flags & fs::ENTRY_DIRECTORY) s += "d";
    if (flags & fs::ENTRY_READ_ONLY) s += "r";
    if (flags & fs::ENTRY_HIDDEN) s += "h";
    if (flags & fs::ENTRY_SYSTEM) s += "s";
    if (flags & fs::ENTRY_PROTECTED) s += "p";
    if (flags & fs::ENTRY_PSX_SAVE) s += "x";
    if (flags & fs::ENTRY_POCKETSTATION) s += "k";
    if (flags & fs::ENTRY_DELETED) s += "!";

    return s;
}

static void enumerate(Instance* iris, FileExplorer* fe) {
    fe->devices.clear();

    for (int slot = 0; slot < 2; slot++) {
        const std::string& path = slot ? iris->paths.mcd1_path : iris->paths.mcd0_path;

        FileExplorerDevice dev;

        dev.kind = FE_DEV_MCD;
        dev.index = slot;
        dev.path = path;
        dev.label = "Memory Card " + std::to_string(slot + 1);
        dev.live = mcd_udata(iris, slot) != nullptr;
        dev.available = dev.live || path.size();
        dev.detail = dev.live ? mcd_type_name(iris->input.mcd_slot_type[slot]) : "";

        fe->devices.push_back(dev);
    }

    for (int port = 0; port < 2; port++) {
        FileExplorerDevice dev;

        dev.kind = FE_DEV_USB;
        dev.index = port;
        dev.path = iris->input.usb_msd_paths[port];
        dev.label = "USB Port " + std::to_string(port + 1);
        dev.live = iris->input.usb_devices[port] == usb::USB_DEVICE_MSD;
        dev.available = dev.path.size();

        fe->devices.push_back(dev);
    }

    FileExplorerDevice disc;

    disc.kind = FE_DEV_DISC;
    disc.path = iris->paths.disc_path;
    disc.label = "Disc";
    disc.live = live_disc(iris) != nullptr;
    disc.available = disc.live || disc.path.size();

    fe->devices.push_back(disc);

    FileExplorerDevice hdd;

    hdd.kind = FE_DEV_HDD;
    hdd.path = iris->paths.hdd_path;
    hdd.label = "HDD";
    hdd.live = iris->ps2 && iris->ps2->speed && iris->ps2->speed->ata;
    hdd.available = hdd.live || hdd.path.size();

    fe->devices.push_back(hdd);

    FileExplorerDevice xfrom;

    xfrom.kind = FE_DEV_XFROM;
    xfrom.path = iris->paths.flash_path;
    xfrom.label = "Flash (XFROM)";
    xfrom.live = live_flash(iris) != nullptr;
    xfrom.available = xfrom.live || xfrom.path.size();
    xfrom.detail = xfrom.live ? "PSX internal flash" : "";

    fe->devices.push_back(xfrom);

    for (const FileExplorerDevice& image : fe->images)
        fe->devices.push_back(image);
}

void FileExplorer::close_device() {
    free_icons();

    fs::close(filesystem);
    fs::blk::close(blk);

    if (disc)
        iop::disc::close(disc);

    filesystem = nullptr;
    blk = nullptr;
    disc = nullptr;
    live_udata = nullptr;
    partition = -1;
    device_size = 0;
    window_offset = 0;
    selected = -1;
    cwd = "/";

    partitions.clear();
    entries.clear();
    visible.clear();
    window.clear();
    preview.clear();
    error.clear();
    list_error.clear();
    preview_error.clear();

    preview_for = -1;
}

void FileExplorer::mount(int partition_index) {
    fs::close(filesystem);

    filesystem = nullptr;
    partition = partition_index;

    if (partition_index < 0) {
        filesystem = fs::probe(iris->logger, blk, false);
    } else if (partition_index < (int)partitions.size()) {
        const fs::part::Partition& p = partitions[partition_index];

        if (p.extents.size())
            filesystem = fs::pfs::open(iris->logger, blk, p.extents, false);

        if (!filesystem) {
            fs::blk::Device* slice = fs::blk::open_slice(iris->logger, blk, p.offset, p.size, false);

            filesystem = fs::probe(iris->logger, slice, true);

            if (!filesystem)
                fs::blk::close(slice);
        }
    }

    navigate("/");
}

void FileExplorer::refresh() {
    selected = -1;
    preview_for = -1;

    entries.clear();
    visible.clear();
    preview.clear();
    list_error.clear();
    preview_error.clear();

    if (!filesystem)
        return;

    int r = fs::list(filesystem, cwd.c_str(), &entries);

    if (r < 0) {
        list_error = fs::error_name(r);

        return;
    }

    apply_filter();
}

static bool contains_nocase(const char* haystack, const char* needle) {
    if (!*needle)
        return true;

    for (const char* h = haystack; *h; h++) {
        const char* a = h;
        const char* b = needle;

        while (*a && *b) {
            char ca = *a, cb = *b;

            if (ca >= 'A' && ca <= 'Z') ca += 32;
            if (cb >= 'A' && cb <= 'Z') cb += 32;

            if (ca != cb)
                break;

            a++;
            b++;
        }

        if (!*b)
            return true;
    }

    return false;
}

void FileExplorer::apply_filter() {
    std::string was;

    if (selected >= 0 && selected < (int)visible.size())
        was = entries[visible[selected]].name;

    visible.clear();

    for (size_t i = 0; i < entries.size(); i++) {
        const fs::Entry& e = entries[i];

        if (!show_deleted && (e.flags & fs::ENTRY_DELETED))
            continue;

        if (!show_hidden && (e.flags & (fs::ENTRY_HIDDEN | fs::ENTRY_SYSTEM)))
            continue;

        if (!contains_nocase(e.name, filter))
            continue;

        visible.push_back((int)i);
    }

    std::stable_sort(visible.begin(), visible.end(), [this](int lhs, int rhs) {
        const fs::Entry& a = entries[lhs];
        const fs::Entry& b = entries[rhs];

        bool da = a.flags & fs::ENTRY_DIRECTORY;
        bool db = b.flags & fs::ENTRY_DIRECTORY;

        if (da != db)
            return da;

        int order = 0;

        switch (sort_column) {
            case 1: order = a.size < b.size ? -1 : a.size > b.size ? 1 : 0; break;
            case 2: order = compare_time(a.modified, b.modified); break;
            case 3: order = compare_time(a.created, b.created); break;
            default: order = strcmp(a.name, b.name); break;
        }

        if (!order)
            order = strcmp(a.name, b.name);

        return sort_ascending ? order < 0 : order > 0;
    });

    selected = -1;

    if (was.size()) {
        for (size_t i = 0; i < visible.size(); i++) {
            if (was == entries[visible[i]].name) {
                selected = (int)i;

                break;
            }
        }
    }

    preview_for = -1;
}

void FileExplorer::free_icons() {
    for (SaveIcon& icon : icons) {
        for (int i = 0; i < icon.frame_count; i++)
            vulkan::free_texture(iris, icon.frames[i]);
    }

    icons.clear();
}

FileExplorer::SaveIcon* FileExplorer::save_icon() {
    if (!filesystem || filesystem->type != fs::FS_PS1_MCD)
        return nullptr;

    if (selected < 0 || selected >= (int)visible.size())
        return nullptr;

    std::string name = entries[visible[selected]].name;

    for (SaveIcon& icon : icons) {
        if (icon.name == name)
            return &icon;
    }

    fs::ps1mcd::SaveInfo info;

    std::string path = fs::path_join(cwd, name);

    if (fs::ps1mcd::get_save_info(filesystem, path.c_str(), &info) < 0)
        return nullptr;

    SaveIcon icon;

    icon.name = name;
    icon.title = info.title;

    for (int i = 0; i < info.icon_frames; i++) {
        icon.frames[i] = vulkan::upload_texture(iris, info.icon[i], 16, 16, 4);

        if (!icon.frames[i].descriptor_set)
            break;

        icon.frame_count = i + 1;
    }

    icons.push_back(icon);

    return &icons.back();
}

void FileExplorer::load_preview() {
    preview.clear();
    preview_error.clear();

    preview_for = selected;

    if (!filesystem || selected < 0 || selected >= (int)visible.size())
        return;

    const fs::Entry& e = entries[visible[selected]];

    if (e.flags & fs::ENTRY_DIRECTORY)
        return;

    fs::Handle* handle = nullptr;

    std::string path = fs::path_join(cwd, e.name);

    int r = fs::open(filesystem, path.c_str(), &handle);

    if (r < 0) {
        preview_error = fs::error_name(r);

        return;
    }

    uint64_t size = std::min<uint64_t>(e.size, PREVIEW_LIMIT);

    preview.resize(size);

    if (size && fs::read(filesystem, handle, 0, preview.data(), size) < 0) {
        preview.clear();

        preview_error = "Read failed";
    }

    fs::close_handle(filesystem, handle);
}

void FileExplorer::navigate(const std::string& path) {
    cwd = path.empty() ? "/" : path;

    refresh();
}

void FileExplorer::seek(uint64_t offset) {
    window.clear();

    if (!blk)
        return;

    if (offset >= device_size)
        offset = device_size ? device_size - 1 : 0;

    offset &= ~(uint64_t)0xf;

    uint64_t size = std::min(WINDOW_SIZE, device_size - offset);

    window.resize(size);

    if (fs::blk::read(blk, offset, window.data(), size) < 0) {
        window.clear();

        error = "Read failed at offset " + std::to_string(offset);

        return;
    }

    window_offset = offset;
}

void FileExplorer::open_device(const FileExplorerDevice& dev) {
    close_device();

    device = dev;

    if (!dev.supported) {
        error = "Browsing this device is not supported yet.";

        return;
    }

    fs::blk::Device* raw = nullptr;

    if (dev.kind == FE_DEV_MCD) {
        live_udata = mcd_udata(iris, dev.index);

        if (live_udata && iris->input.mcd_slot_type[dev.index] == 1) {
            dev::mcd::Mcd* mcd = (dev::mcd::Mcd*)live_udata;

            raw = fs::blk::open_memory(iris->logger, mcd->buf, mcd->buf_size, false);
        } else if (live_udata) {
            dev::ps1_mcd::Ps1Mcd* mcd = (dev::ps1_mcd::Ps1Mcd*)live_udata;

            raw = fs::blk::open_memory(iris->logger, mcd->buf, dev::ps1_mcd::SIZE, false);
        }
    }

    if (dev.kind == FE_DEV_DISC || (dev.kind == FE_DEV_IMAGE && looks_like_disc(dev.path))) {
        iop::disc::Disc* live = dev.kind == FE_DEV_DISC ? live_disc(iris) : nullptr;

        if (live) {
            raw = fs::blk::open_disc(iris->logger, live);
        } else if (dev.path.size()) {
            disc = iop::disc::open(dev.path.c_str());

            if (disc)
                raw = fs::blk::open_disc(iris->logger, disc);
        }

        if (!raw && disc) {
            iop::disc::close(disc);

            disc = nullptr;
        }
    }

    if (dev.kind == FE_DEV_HDD && iris->ps2 && iris->ps2->speed && iris->ps2->speed->ata)
        raw = fs::blk::open_ata(iris->logger, &iris->ps2->speed->ata->hdd);

    if (dev.kind == FE_DEV_XFROM) {
        live_udata = live_flash(iris);

        if (live_udata) {
            speed::flash::Flash* flash = (speed::flash::Flash*)live_udata;

            raw = fs::blk::open_memory(iris->logger, flash->file, speed::flash::CARD_SIZE_ECC, false);
        }
    }

    if (!raw && !disc && dev.path.size())
        raw = fs::blk::open_file(iris->logger, dev.path.c_str());

    if (!raw) {
        error = dev.path.size() ? "Could not open \"" + dev.path + "\"" : "Nothing to open.";

        return;
    }

    if (hide_ecc) {
        fs::blk::Device* stripped = fs::blk::open_ecc(iris->logger, raw, 512, 16, true);

        if (!stripped) {
            fs::blk::close(raw);

            error = "Image is too small to have an ECC area.";

            return;
        }

        raw = stripped;
    }

    blk = raw;
    device_size = fs::blk::get_size(blk);
    last_device = file_explorer_device_key(dev);

    mount(-1);

    if (!filesystem && fs::part::scan(iris->logger, blk, &partitions)) {
        for (size_t i = 0; i < partitions.size(); i++) {
            mount((int)i);

            if (filesystem)
                break;
        }

        if (!filesystem) {
            partition = -1;

            navigate("/");
        }
    }

    seek(0);
}

static bool extract_file(FileExplorer* fe, const std::string& vpath, const std::filesystem::path& host, std::string* error) {
    fs::Entry st;

    int r = fs::stat(fe->filesystem, vpath.c_str(), &st);

    if (r < 0) {
        *error = fs::error_name(r);

        return false;
    }

    fs::Handle* handle = nullptr;

    r = fs::open(fe->filesystem, vpath.c_str(), &handle);

    if (r < 0) {
        *error = fs::error_name(r);

        return false;
    }

    std::ofstream out(host, std::ios::binary);

    if (!out) {
        fs::close_handle(fe->filesystem, handle);

        *error = "could not create the output file";

        return false;
    }

    std::vector <uint8_t> buf(256 * 1024);

    uint64_t offset = 0;
    bool ok = true;

    while (offset < st.size) {
        uint64_t chunk = std::min<uint64_t>(buf.size(), st.size - offset);

        int64_t got = fs::read(fe->filesystem, handle, offset, buf.data(), chunk);

        if (got <= 0) {
            *error = "read failed";
            ok = false;

            break;
        }

        out.write((const char*)buf.data(), got);

        if (!out) {
            *error = "write failed";
            ok = false;

            break;
        }

        offset += (uint64_t)got;
    }

    fs::close_handle(fe->filesystem, handle);

    return ok;
}

static bool extract_tree(FileExplorer* fe, const std::string& vpath, const std::filesystem::path& host,
    const std::filesystem::path& root, int depth, int* files, int* skipped, std::string* error) {
    if (depth > MAX_EXTRACT_DEPTH) {
        *error = "directory nesting is too deep";

        return false;
    }

    std::vector <fs::Entry> listing;

    int r = fs::list(fe->filesystem, vpath.c_str(), &listing);

    if (r < 0) {
        *error = fs::error_name(r);

        return false;
    }

    std::error_code ec;

    std::filesystem::create_directories(host, ec);

    if (ec) {
        *error = "could not create the output directory";

        return false;
    }

    for (const fs::Entry& e : listing) {
        std::string safe;

        if (!fs::sanitize_name(e.name, &safe)) {
            (*skipped)++;

            continue;
        }

        std::filesystem::path target = host / safe;

        if (!fs::path_is_inside(root.string(), target.string())) {
            (*skipped)++;

            continue;
        }

        std::string child = fs::path_join(vpath, e.name);

        if (e.flags & fs::ENTRY_DIRECTORY) {
            if (!extract_tree(fe, child, target, root, depth + 1, files, skipped, error))
                return false;
        } else if (extract_file(fe, child, target, error)) {
            (*files)++;
        } else {
            return false;
        }
    }

    return true;
}

static void extract_selected(Instance* iris, FileExplorer* fe, const std::string& name, bool directory) {
    std::string vpath = fs::path_join(fe->cwd, name);
    std::string start = fe->last_extract_dir.size() ? fe->last_extract_dir : iris->paths.pref_path;

    audio::mute(iris);

    std::string chosen;

    if (directory) {
        auto f = pfd::select_folder("Extract " + name + " to", start);

        while (!f.ready());

        chosen = f.result();
    } else {
        std::string safe;

        if (!fs::sanitize_name(name.c_str(), &safe))
            safe = "extracted.bin";

        auto f = pfd::save_file("Extract " + name, start + safe, { "All Files (*.*)", "*" });

        while (!f.ready());

        chosen = f.result();
    }

    audio::unmute(iris);

    if (chosen.empty())
        return;

    std::filesystem::path target = chosen;
    std::string error;

    int files = 0;
    int skipped = 0;
    bool ok;

    if (directory) {
        std::string safe;

        fs::sanitize_name(name.c_str(), &safe);

        target /= safe;

        ok = extract_tree(fe, vpath, target, target, 0, &files, &skipped, &error);

        fe->last_extract_dir = chosen;
    } else {
        ok = extract_file(fe, vpath, target, &error);

        files = ok ? 1 : 0;
        fe->last_extract_dir = target.parent_path().string();
    }

    if (fe->last_extract_dir.size() && fe->last_extract_dir.back() != '/' && fe->last_extract_dir.back() != '\\')
        fe->last_extract_dir += "/";

    if (!ok) {
        iris_error(&iris->log.ui, "Extracting \"{}\" failed: {}", vpath, error);

        push_info(iris, "Extract failed: " + error);

        return;
    }

    std::string msg = "Extracted " + std::to_string(files) + (files == 1 ? " file" : " files");

    if (skipped)
        msg += ", skipped " + std::to_string(skipped) + " with unusable names";

    push_info(iris, msg);
}

static void show_menubar(Instance* iris, FileExplorer* fe) {
    using namespace ImGui;

    if (!BeginMenuBar())
        return;

    if (imgui::BeginMenu("File")) {
        if (imgui::MenuItem(ICON_MS_FILE_OPEN " Open image...")) {
            audio::mute(iris);

            auto f = pfd::open_file("Open image", "", {
                "All supported images", "*.mcd *.ps2 *.psm *.mcr *.bin *.img *.raw *.iso *.cue *.chd *.cso *.zso",
                "Memory cards (*.mcd; *.ps2; *.psm; *.mcr)", "*.mcd *.ps2 *.psm *.mcr",
                "Disc images (*.iso; *.bin; *.cue; *.chd; *.cso; *.zso)", "*.iso *.bin *.cue *.chd *.cso *.zso",
                "Drive images (*.img; *.raw)", "*.img *.raw",
                "All Files (*.*)", "*"
            });

            while (!f.ready());

            audio::unmute(iris);

            if (f.result().size()) {
                FileExplorerDevice dev;

                dev.kind = FE_DEV_IMAGE;
                dev.path = f.result().at(0);
                dev.label = dev.path.substr(dev.path.find_last_of("/\\") + 1);
                dev.available = true;

                fe->images.push_back(dev);
                fe->open_device(dev);
            }
        }

        if (imgui::MenuItem(ICON_MS_REFRESH " Refresh", "F5", false, fe->blk != nullptr))
            fe->open_device(fe->device);

        bool has_selection = fe->filesystem && fe->selected >= 0 && fe->selected < (int)fe->visible.size();

        if (imgui::MenuItem(ICON_MS_FILE_DOWNLOAD " Extract selected...", nullptr, false, has_selection)) {
            const fs::Entry& e = fe->entries[fe->visible[fe->selected]];

            extract_selected(iris, fe, e.name, e.flags & fs::ENTRY_DIRECTORY);
        }

        Separator();

        if (imgui::MenuItem(ICON_MS_CLOSE " Close device", nullptr, false, fe->blk != nullptr))
            fe->close_device();

        ImGui::EndMenu();
    }

    if (imgui::BeginMenu("View")) {
        imgui::MenuItem(ICON_MS_VISIBILITY " Preview pane", nullptr, &fe->show_preview);

        if (imgui::MenuItem(ICON_MS_VISIBILITY_OFF " Show hidden", nullptr, &fe->show_hidden))
            fe->apply_filter();

        if (imgui::MenuItem(ICON_MS_DELETE " Show deleted", nullptr, &fe->show_deleted))
            fe->apply_filter();

        imgui::MenuItem(ICON_MS_DATA_OBJECT " Raw image", nullptr, &fe->raw_view, fe->filesystem != nullptr);

        if (imgui::MenuItem(ICON_MS_SDK " Hide ECC area", nullptr, &fe->hide_ecc)) {
            if (fe->blk)
                fe->open_device(fe->device);
        }

        ImGui::EndMenu();
    }

    EndMenuBar();
}

static std::string basename(const std::string& path) {
    size_t slash = path.find_last_of("/\\");

    return slash == std::string::npos ? path : path.substr(slash + 1);
}

static std::string elide(const std::string& text, float width) {
    if (width <= 0.0f || ImGui::CalcTextSize(text.c_str()).x <= width)
        return text;

    std::string out = text;

    while (out.size()) {
        out.pop_back();

        while (out.size() && ((unsigned char)out.back() & 0xc0) == 0x80)
            out.pop_back();

        if (ImGui::CalcTextSize((out + "...").c_str()).x <= width)
            break;
    }

    return out + "...";
}

static std::string device_subtitle(const FileExplorerDevice& dev) {
    if (!dev.supported)
        return "Not supported yet";

    if (!dev.available)
        return "Not configured";

    if (dev.live && dev.detail.size())
        return dev.detail;

    return dev.path.size() ? basename(dev.path) : "Inserted";
}

static void show_sidebar(Instance* iris, FileExplorer* fe) {
    using namespace ImGui;

    ImGuiStyle& style = GetStyle();
    ImDrawList* draw = GetWindowDrawList();

    float line = GetTextLineHeight();
    float row_height = line * 2.0f + style.FramePadding.y * 2.0f + 2.0f;

    for (size_t i = 0; i < fe->devices.size(); i++) {
        const FileExplorerDevice& dev = fe->devices[i];

        if (i && dev.kind != fe->devices[i - 1].kind)
            Separator();

        bool selected = fe->blk && file_explorer_device_key(dev) == fe->last_device;

        BeginDisabled(!dev.available || !dev.supported);

        ImVec2 origin = GetCursorScreenPos();
        float width = GetContentRegionAvail().x;

        if (imgui::Selectable(("##dev" + std::to_string(i)).c_str(), selected, 0, ImVec2(0, row_height)))
            fe->open_device(dev);

        if (!dev.supported) {
            SetItemTooltip("Browsing this device is not supported yet");
        } else if (dev.path.size()) {
            std::string tip = dev.detail.size() ? dev.detail + "\n" + dev.path : dev.path;

            SetItemTooltip("%s", tip.c_str());
        }

        const char* glyph =
            dev.kind == FE_DEV_MCD ? ICON_MS_SD_CARD :
            dev.kind == FE_DEV_USB ? ICON_MS_USB :
            dev.kind == FE_DEV_DISC ? ICON_MS_ALBUM :
            dev.kind == FE_DEV_HDD ? ICON_MS_HARD_DRIVE :
            dev.kind == FE_DEV_XFROM ? ICON_MS_MEMORY : ICON_MS_FOLDER_ZIP;

        ImU32 normal = GetColorU32(ImGuiCol_Text);
        ImU32 dim = GetColorU32(ImGuiCol_TextDisabled);

        float x = origin.x + style.FramePadding.x;
        float y = origin.y + style.FramePadding.y;
        float right = origin.x + width - style.FramePadding.x;

        float dot = line * 0.2f;
        float tag = dev.live ? dot * 2.0f + style.ItemInnerSpacing.x * 2.0f : 0.0f;

        std::string title = elide(std::string(glyph) + "  " + dev.label,
            width - style.FramePadding.x * 2.0f - tag);

        draw->AddText(ImVec2(x, y), normal, title.c_str());

        if (dev.live)
            draw->AddCircleFilled(ImVec2(right - dot, y + line * 0.5f), dot, GetColorU32(IM_COL32(90, 200, 110, 255)));

        float indent = CalcTextSize(glyph).x + CalcTextSize("  ").x;

        std::string subtitle = elide(device_subtitle(dev), width - indent - style.FramePadding.x * 2.0f);

        draw->AddText(ImVec2(x + indent, y + line), dim, subtitle.c_str());

        EndDisabled();
    }
}

static void show_raw_view(Instance* iris, FileExplorer* fe) {
    using namespace ImGui;

    BeginDisabled(!fe->window_offset);

    if (Button(ICON_MS_ARROW_UPWARD "##page_up"))
        fe->seek(fe->window_offset < WINDOW_SIZE ? 0 : fe->window_offset - WINDOW_SIZE);

    EndDisabled();

    SameLine();

    BeginDisabled(fe->window_offset + WINDOW_SIZE >= fe->device_size);

    if (Button(ICON_MS_ARROW_DOWNWARD "##page_down"))
        fe->seek(fe->window_offset + WINDOW_SIZE);

    EndDisabled();

    SameLine();

    char offset[32];

    snprintf(offset, sizeof(offset), "%llx", (unsigned long long)fe->window_offset);

    SetNextItemWidth(140.0f);

    if (InputText("##offset", offset, sizeof(offset), ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_EnterReturnsTrue))
        fe->seek(strtoull(offset, nullptr, 16));

    SameLine();
    TextDisabled("offset");

    if (fe->window.empty())
        return;

    editor.ReadOnly = true;
    editor.OptShowOptions = false;
    editor.FontOptions = iris->ui.font_body;

    PushFont(iris->ui.font_code);

    editor.DrawContents(fe->window.data(), fe->window.size(), (size_t)fe->window_offset);

    PopFont();
}

static void show_breadcrumb(FileExplorer* fe, Action* action) {
    using namespace ImGui;

    if (SmallButton(ICON_MS_HOME "##crumb_root"))
        action->go = "/";

    std::string path;

    std::vector <std::string> parts = fs::path_split(fe->cwd);

    for (size_t i = 0; i < parts.size(); i++) {
        path += "/" + parts[i];

        SameLine(0.0f, 4.0f);
        TextDisabled(ICON_MS_CHEVRON_RIGHT);
        SameLine(0.0f, 4.0f);

        if (SmallButton((parts[i] + "##crumb" + std::to_string(i)).c_str()))
            action->go = path;
    }
}

static void show_listing(FileExplorer* fe, Action* action) {
    using namespace ImGui;

    if (fe->list_error.size()) {
        imgui::TextDisabledCentered(ICON_MS_ERROR " %s", fe->list_error.c_str());

        return;
    }

    if (fe->entries.empty()) {
        imgui::TextDisabledCentered("This folder is empty.");

        return;
    }

    if (fe->visible.empty()) {
        imgui::TextDisabledCentered("Nothing matches the filter.");

        return;
    }

    ImGuiTableFlags flags =
        ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable |
        ImGuiTableFlags_Sortable | ImGuiTableFlags_SortTristate |
        ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_BordersInnerV;

    if (!BeginTable("##fe_files", 5, flags))
        return;

    TableSetupScrollFreeze(0, 1);
    TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_DefaultSort, 3.0f);
    TableSetupColumn("Size", ImGuiTableColumnFlags_WidthStretch, 1.0f);
    TableSetupColumn("Modified", ImGuiTableColumnFlags_WidthStretch, 2.0f);
    TableSetupColumn("Created", ImGuiTableColumnFlags_WidthStretch, 2.0f);
    TableSetupColumn("Flags", ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_NoSort, 1.0f);
    TableHeadersRow();

    if (ImGuiTableSortSpecs* specs = TableGetSortSpecs()) {
        if (specs->SpecsDirty && specs->SpecsCount) {
            fe->sort_column = specs->Specs[0].ColumnIndex;
            fe->sort_ascending = specs->Specs[0].SortDirection == ImGuiSortDirection_Ascending;

            fe->apply_filter();

            specs->SpecsDirty = false;
        }
    }

    ImGuiListClipper clipper;

    clipper.Begin((int)fe->visible.size());

    while (clipper.Step()) {
        for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
            const fs::Entry& e = fe->entries[fe->visible[i]];

            bool dir = e.flags & fs::ENTRY_DIRECTORY;

            TableNextRow();
            TableSetColumnIndex(0);

            std::string label = std::string(dir ? ICON_MS_FOLDER : ICON_MS_DESCRIPTION) + " " + e.name +
                "##row" + std::to_string(i);

            if (imgui::Selectable(label.c_str(), i == fe->selected,
                ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowDoubleClick)) {
                fe->selected = i;

                if (dir && IsMouseDoubleClicked(0))
                    action->go = fs::path_join(fe->cwd, e.name);
            }

            if (BeginPopupContextItem(("##ctx" + std::to_string(i)).c_str())) {
                fe->selected = i;

                if (imgui::MenuItem(dir ? ICON_MS_FOLDER_COPY " Extract folder..." : ICON_MS_FILE_DOWNLOAD " Extract...")) {
                    action->extract = e.name;
                    action->directory = dir;
                }

                Separator();

                if (imgui::MenuItem(ICON_MS_CONTENT_COPY " Copy name"))
                    SDL_SetClipboardText(e.name);

                if (imgui::MenuItem(ICON_MS_CONTENT_COPY " Copy path"))
                    SDL_SetClipboardText(fs::path_join(fe->cwd, e.name).c_str());

                EndPopup();
            }

            TableSetColumnIndex(1);

            if (dir) {
                TextDisabled("--");
            } else {
                TextUnformatted(imgui::format_size(e.size).c_str());
            }

            TableSetColumnIndex(2);
            TextUnformatted(format_time(e.modified).c_str());

            TableSetColumnIndex(3);
            TextUnformatted(format_time(e.created).c_str());

            TableSetColumnIndex(4);
            TextDisabled("%s", format_flags(e.flags).c_str());
        }
    }

    EndTable();
}

static void show_preview(Instance* iris, FileExplorer* fe) {
    using namespace ImGui;

    if (fe->selected != fe->preview_for)
        fe->load_preview();

    if (!BeginTabBar("##fe_preview"))
        return;

    const fs::Entry* e =
        fe->selected >= 0 && fe->selected < (int)fe->visible.size() ? &fe->entries[fe->visible[fe->selected]] : nullptr;

    if (BeginTabItem("Info")) {
        FileExplorer::SaveIcon* icon = e ? fe->save_icon() : nullptr;

        if (icon) {
            if (icon->frame_count) {
                // Roughly the rate the PS1 browser animates at
                int frame = icon->frame_count > 1 ? (int)(GetTime() * 5.0) % icon->frame_count : 0;

                Image((ImTextureID)(intptr_t)icon->frames[frame].descriptor_set, ImVec2(64, 64));

                SameLine();
            }

            BeginGroup();

            PushFont(iris->ui.font_heading);
            TextUnformatted(icon->title.size() ? icon->title.c_str() : "(untitled save)");
            PopFont();

            TextDisabled("%s", e->name);

            EndGroup();

            Separator();
        }

        if (!e) {
            imgui::TextDisabledCentered("Select an entry.");
        } else if (BeginTable("##fe_info", 2, ImGuiTableFlags_SizingFixedFit)) {
            auto row = [](const char* key, const std::string& value) {
                TableNextRow();
                TableSetColumnIndex(0);
                TextDisabled("%s", key);
                TableSetColumnIndex(1);
                TextUnformatted(value.c_str());
            };

            row("Name", e->name);
            row("Path", fs::path_join(fe->cwd, e->name));
            row("Type", (e->flags & fs::ENTRY_DIRECTORY) ? "Directory" : "File");

            if (!(e->flags & fs::ENTRY_DIRECTORY))
                row("Size", imgui::format_size(e->size) + " (" + std::to_string(e->size) + " bytes)");

            if (e->created.valid)
                row("Created", format_time(e->created));

            if (e->modified.valid)
                row("Modified", format_time(e->modified));

            std::string flags = format_flags(e->flags);

            if (flags.size())
                row("Flags", flags);

            EndTable();
        }

        EndTabItem();
    }
    if (BeginTabItem("Hex")) {
        if (!e) {
            imgui::TextDisabledCentered("Select a file.");
        } else if (e->flags & fs::ENTRY_DIRECTORY) {
            imgui::TextDisabledCentered("Directories have no contents to preview.");
        } else if (fe->preview_error.size()) {
            imgui::TextDisabledCentered(ICON_MS_ERROR " %s", fe->preview_error.c_str());
        } else if (fe->preview.empty()) {
            imgui::TextDisabledCentered("This file is empty.");
        } else {
            if (e->size > fe->preview.size())
                TextDisabled("Showing the first %s of %s", imgui::format_size(fe->preview.size()).c_str(),
                    imgui::format_size(e->size).c_str());

            editor.ReadOnly = true;
            editor.OptShowOptions = false;
            editor.FontOptions = iris->ui.font_body;

            PushFont(iris->ui.font_code);

            editor.DrawContents(fe->preview.data(), fe->preview.size(), 0);

            PopFont();
        }

        EndTabItem();
    }


    EndTabBar();
}

static void show_browser(Instance* iris, FileExplorer* fe) {
    using namespace ImGui;

    Action action;

    BeginDisabled(fe->cwd == "/");

    if (Button(ICON_MS_ARROW_UPWARD "##up"))
        action.go = fs::path_parent(fe->cwd);

    EndDisabled();

    SameLine();

    if (Button(ICON_MS_REFRESH "##refresh"))
        fe->refresh();

    SameLine();

    bool has_selection = fe->selected >= 0 && fe->selected < (int)fe->visible.size();

    BeginDisabled(!has_selection);

    if (Button(ICON_MS_FILE_DOWNLOAD "##extract") && has_selection) {
        const fs::Entry& e = fe->entries[fe->visible[fe->selected]];

        action.extract = e.name;
        action.directory = e.flags & fs::ENTRY_DIRECTORY;
    }

    EndDisabled();

    SetItemTooltip("Extract the selected entry");

    SameLine(); SeparatorEx(ImGuiSeparatorFlags_Vertical); SameLine();

    show_breadcrumb(fe, &action);

    float filter_width = 200.0f;

    SameLine(std::max(GetCursorPosX(), GetContentRegionMax().x - filter_width));

    SetNextItemWidth(filter_width);

    if (InputTextWithHint("##fe_filter", ICON_MS_SEARCH " Filter", fe->filter, sizeof(fe->filter)))
        fe->apply_filter();

    float avail = GetContentRegionAvail().y;

    if (fe->show_preview) {
        fe->preview_height = std::clamp(fe->preview_height, 100.0f, std::max(120.0f, avail - 120.0f));

        float list_h = avail - fe->preview_height - GetStyle().ItemSpacing.y;

        if (BeginChild("##fe_list", ImVec2(0, list_h)))
            show_listing(fe, &action);

        EndChild();

        float width = GetContentRegionAvail().x;

        imgui::splitter("##fe_split_preview", false, imgui::splitter_at_cursor(false), &list_h, &fe->preview_height, 120.0f, 100.0f, width);

        if (BeginChild("##fe_preview_pane", ImVec2(0, 0), ImGuiChildFlags_Borders))
            show_preview(iris, fe);

        EndChild();
    } else {
        show_listing(fe, &action);
    }

    if (action.extract.size())
        extract_selected(iris, fe, action.extract, action.directory);

    if (action.go.size())
        fe->navigate(action.go);
}

static std::string partition_label(FileExplorer* fe, int index) {
    if (index < 0 || index >= (int)fe->partitions.size())
        return "Whole device";

    const fs::part::Partition& p = fe->partitions[index];

    std::string label = "Partition " + std::to_string(index + 1);

    if (p.name[0])
        label += " \xc2\xb7 " + std::string(p.name);

    if (p.type_name[0])
        label += " \xc2\xb7 " + std::string(p.type_name);

    return label + " \xc2\xb7 " + imgui::format_size(p.size);
}

static void show_content(Instance* iris, FileExplorer* fe) {
    using namespace ImGui;

    if (fe->error.size()) {
        PushStyleColor(ImGuiCol_Text, GetStyleColorVec4(ImGuiCol_TextDisabled));
        TextWrapped(ICON_MS_ERROR " %s", fe->error.c_str());
        PopStyleColor();

        return;
    }

    if (!fe->blk) {
        imgui::TextDisabledCentered("Select a device.");

        return;
    }

    PushFont(iris->ui.font_heading);
    TextUnformatted(fe->device.label.c_str());
    PopFont();

    std::string subtitle = imgui::format_size(fe->device_size);

    if (fe->filesystem) {
        subtitle += " \xc2\xb7 ";
        subtitle += fs::type_name(fe->filesystem);

        if (fe->filesystem->label[0])
            subtitle += " \xc2\xb7 " + std::string(fe->filesystem->label);

        if (fe->filesystem->free_bytes)
            subtitle += " \xc2\xb7 " + imgui::format_size(fe->filesystem->free_bytes) + " free";
    } else if (fe->device.detail.size()) {
        subtitle += " \xc2\xb7 " + fe->device.detail;
    }

    TextDisabled("%s", subtitle.c_str());

    if (fe->partitions.size() > 1) {
        SetNextItemWidth(280.0f);

        if (BeginCombo("##fe_partition", partition_label(fe, fe->partition).c_str())) {
            for (size_t i = 0; i < fe->partitions.size(); i++) {
                if (imgui::Selectable(partition_label(fe, (int)i).c_str(), (int)i == fe->partition))
                    fe->mount((int)i);
            }

            EndCombo();
        }
    }

    Separator();

    if (!fe->filesystem) {
        if (fe->partitions.size()) {
            TextDisabled(ICON_MS_ERROR " Found %d partition(s), none with a filesystem Iris can read.",
                (int)fe->partitions.size());
        } else {
            TextDisabled(ICON_MS_ERROR " No recognised filesystem. Showing the raw image.");
        }

        show_raw_view(iris, fe);

        return;
    }

    if (fe->raw_view) {
        show_raw_view(iris, fe);

        return;
    }

    show_browser(iris, fe);
}

void browse_device(Instance* iris, int kind, int index) {
    FileExplorer& fe = iris->applets.file_explorer;

    fe.pending = true;
    fe.pending_kind = kind;
    fe.pending_index = index;

    fe.show();
}

bool FileExplorer::begin() {
    ImGui::SetNextWindowSizeConstraints(ImVec2(560, 360), ImVec2(FLT_MAX, FLT_MAX));

    return Applet::begin();
}

void FileExplorer::on_open() {
    restore = last_device.size() && !blk;
}

void FileExplorer::on_close() {
    close_device();

    images.clear();
}

void FileExplorer::on_render() {
    using namespace ImGui;

    if (blk && device.kind == FE_DEV_MCD && mcd_udata(iris, device.index) != live_udata)
        open_device(device);

    if (blk && device.kind == FE_DEV_XFROM && live_flash(iris) != live_udata)
        open_device(device);

    enumerate(iris, this);

    if (pending) {
        pending = false;
        restore = false;

        for (const FileExplorerDevice& dev : devices) {
            if (dev.kind == pending_kind && dev.index == pending_index && dev.available && dev.supported) {
                open_device(dev);

                break;
            }
        }
    }

    if (restore) {
        restore = false;

        for (const FileExplorerDevice& dev : devices) {
            if (dev.available && dev.supported && file_explorer_device_key(dev) == last_device) {
                open_device(dev);

                break;
            }
        }
    }

    show_menubar(iris, this);

    if (blk && IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && IsKeyPressed(ImGuiKey_F5, false))
        open_device(device);

    ImVec2 avail = GetContentRegionAvail();

    sidebar_width = std::clamp(sidebar_width, 160.0f, std::max(180.0f, avail.x - 300.0f));

    float rest = avail.x - sidebar_width;

    imgui::splitter("##fe_split", true, imgui::splitter_before(true, sidebar_width), &sidebar_width, &rest, 160.0f, 300.0f, avail.y);

    if (BeginChild("##fe_devices", ImVec2(sidebar_width, 0), ImGuiChildFlags_Borders))
        show_sidebar(iris, this);

    EndChild();

    SameLine();

    if (BeginChild("##fe_content", ImVec2(0, 0)))
        show_content(iris, this);

    EndChild();
}

}
