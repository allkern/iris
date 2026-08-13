#include "iris.hpp"
#include "arcade.hpp"
#include "slirp.hpp"

#include "miniz.h"
#include "ps2.hpp"
#include "fs/mkfs.hpp"

#include <filesystem>
#include <algorithm>
#include <optional>
#include <cctype>
#include <thread>

namespace iris::emu {

bool init(Instance* iris) {
    // Initialize our emulator state
    iris->ps2 = ps2::create(iris->logger);

    ps2::init(iris->ps2);
    ps2::init_tty_handler(iris->ps2, ps2::EE, handle_ee_tty_event, iris);
    ps2::init_tty_handler(iris->ps2, ps2::IOP, handle_iop_tty_event, iris);
    ps2::init_tty_handler(iris->ps2, ps2::SYSMEM, handle_sysmem_tty_event, iris);

    iris->input.ds[0] = dev::ds::attach(iris->logger, iris->ps2->sio2, 0);

    return true;
}

void destroy(Instance* iris) {
    slirp::stop();

    if (iris->ps2) ps2::destroy(iris->ps2);
}

const char* get_extension(const char* path) {
    const char* dot = strrchr(path, '.');

    if (!dot || dot == path)
        return nullptr;

    return dot + 1;
}

static void finish_load(Instance* iris, int result, std::string name = "") {
    iris->load_result = result;
    iris->load_pending_name = std::move(name);
    iris->load_ready.store(true, std::memory_order_release);
}

void finalize_load(Instance* iris) {
    vulkan::wait_idle(iris);

    gs::renderer::hotswap(iris->renderer, iris->renderer_backend);

    iris->ui.loading_file_active = false;
    iris->ui.loading_target = "";
    iris->ui.show_gamelist = false;

    imgui::end_dim(iris);

    if (iris->load_result == 0) {
        gs::renderer::reset(iris->renderer);

        iris->vk.image = {};

        iris->loaded = iris->load_pending_name;

        if (iris->autostart)
            iris->debug.pause = false;
    }
}

int open_archive(Instance* iris, std::string path) {
    mz_zip_archive zip;

    mz_zip_zero_struct(&zip);

    if (!mz_zip_reader_init_file(&zip, path.c_str(), 0)) {
        iris_error(&iris->log.emu, "Couldn't open archive \"{}\"", path.c_str());

        return 1;
    }

    // Decompress everything into pref_path/tmp/
    std::filesystem::path tmp_path = std::filesystem::path(iris->paths.pref_path) / "tmp";

    std::error_code ec;
    std::filesystem::create_directories(tmp_path, ec);

    mz_uint count = mz_zip_reader_get_num_files(&zip);

    for (mz_uint i = 0; i < count; i++) {
        mz_zip_archive_file_stat stat;

        if (!mz_zip_reader_file_stat(&zip, i, &stat))
            continue;

        std::filesystem::path dst = tmp_path / stat.m_filename;

        if (mz_zip_reader_is_file_a_directory(&zip, i)) {
            std::filesystem::create_directories(dst, ec);

            continue;
        }

        // Make sure the parent directory exists before extracting
        std::filesystem::create_directories(dst.parent_path(), ec);

        if (!mz_zip_reader_extract_to_file(&zip, i, dst.string().c_str(), 0)) {
            iris_error(&iris->log.emu, "Failed to extract \"{}\" from archive", stat.m_filename);
        }
    }

    mz_zip_reader_end(&zip);

    return 0;
}

bool is_disc_image(const std::string& file) {
    std::string ext = std::filesystem::path(file).extension().string();

    for (char& c : ext)
        c = tolower(c);

    return ext == ".iso" || ext == ".bin" || ext == ".cue" ||
           ext == ".chd" || ext == ".cso" || ext == ".zso";
}

int insert_disc(Instance* iris, std::string file) {
    // 2-second delay to allow the disc to spin up
    if (cdvd::open(iris->ps2->cdvd, file.c_str(), 38860800 * 2))
        return 1;

    iris->loaded = file;

    return 0;
}

int open_file_thread(Instance* iris, std::string file) {
    std::filesystem::path path(file);
    std::string ext = path.extension().string();

    for (char& c : ext)
        c = tolower(c);

    if (ext == ".zip") {
        int res = open_archive(iris, file);

        finish_load(iris, res);

        return res;
    }

    // Load disc image
    if (is_disc_image(file)) {
        if (cdvd::open(iris->ps2->cdvd, file.c_str(), 0)) {
            finish_load(iris, 1);

            return 1;
        }

        char* boot_file = iop::disc::get_boot_path(iris->ps2->cdvd->disc);

        if (!boot_file) {
            finish_load(iris, 2);

            return 2;
        }

        elf::load_symbols_from_disc(iris);

        ps2::set_system(iris->ps2, iris->system);
        emu::load_rom_files(iris);
        ps2::boot_file(iris->ps2, boot_file);

        finish_load(iris, 0, file);

        return 0;
    }

    elf::load_symbols_from_file(iris, file);

    iris->paths.host_elf_dir = std::filesystem::path(file).parent_path().string();

    if (iris->paths.host_from_elf)
        settings::apply_device_maps(iris);

    // Note: We need the trailing whitespaces here because of IOMAN HLE
    // Load executable
    file = "host:  " + file;

    ps2::set_system(iris->ps2, iris->system);
    emu::load_rom_files(iris);
    ps2::boot_file(iris->ps2, file.c_str());

    finish_load(iris, 0, file);

    return 0;
}

int open_file(Instance* iris, std::string file) {
    std::filesystem::path path(file);

    iris->ui.loading_target = path.filename().string();
    iris->ui.loading_file_active = true;
    iris->load_ready = false;
    iris->debug.pause = true;

    gs::renderer::hotswap(iris->renderer, gs::renderer::BACKEND_NULL);

    imgui::start_dim(iris, 0.35f, 100);

    iris->load_pending_file = file;
    iris->load_start_pending = true;

    return 0;
}

static int boot_ps2_path_thread(Instance* iris, std::string path) {
    ps2::set_system(iris->ps2, iris->system);
    emu::load_rom_files(iris);
    ps2::boot_file(iris->ps2, path.c_str());

    finish_load(iris, 0, path);

    return 0;
}

int boot_ps2_path(Instance* iris, std::string path) {
    size_t sep = path.find_last_of("/\\");

    iris->ui.loading_target = sep == std::string::npos ? path : path.substr(sep + 1);
    iris->ui.loading_file_active = true;
    iris->load_ready = false;
    iris->debug.pause = true;

    gs::renderer::hotswap(iris->renderer, gs::renderer::BACKEND_NULL);

    imgui::start_dim(iris, 0.35f, 100);

    iris->load_pending_file = path;
    iris->load_pending_boot = true;
    iris->load_start_pending = true;

    return 0;
}

void start_pending_load(Instance* iris) {
    if (!iris->load_start_pending)
        return;

    iris->load_start_pending = false;

    if (iris->load_pending_boot) {
        iris->load_pending_boot = false;

        std::thread(boot_ps2_path_thread, iris, iris->load_pending_file).detach();

        return;
    }

    std::thread(open_file_thread, iris, iris->load_pending_file).detach();
}

template <typename T> std::optional<T> query_arcade_value(std::string arcade_name, std::string key) {
    auto it = g_arcade_definitions.find(arcade_name);

    if (it == g_arcade_definitions.end())
        return {};

    auto arcade_table = it->second.as_table();

    auto key_it = arcade_table->find(key);

    if (key_it == arcade_table->end())
        return {};

    if constexpr (std::is_same_v<T, std::string>) {
        return key_it->second.as_string()->get();
    } else if constexpr (std::is_integral_v<T>) {
        return key_it->second.as_integer()->get();
    } else if constexpr (std::is_same_v<T, bool>) {
        return key_it->second.as_boolean()->get();
    } else if constexpr (std::is_floating_point_v<T>) {
        return key_it->second.as_floating_point()->get();
    } else if constexpr (std::is_array_v<T>) {
        return key_it->second.as_array();
    } else {
        return {};
    }

    return {};
}

bool load_arcade(Instance* iris, std::string path) {
    std::filesystem::path base_path(path);

    std::string id = base_path.stem().string();
    std::string name = query_arcade_value<std::string>(id, "name").value_or("");

    if (!name.size()) {
        return false;
    }

    iris_info(&iris->log.emu, "Loading arcade game \"{}\" ({})...", name.c_str(), id.c_str());

    int system = query_arcade_value<int>(id, "system").value_or(ps2::AUTO);

    switch (system) {
        case ps2::NAMCO_SYSTEM_147:
        case ps2::NAMCO_SYSTEM_148: {
            std::string bios = query_arcade_value<std::string>(id, "bios").value_or("");
            std::string nand = query_arcade_value<std::string>(id, "nand").value_or("");

            int ioboard_mode = query_arcade_value<int>(id, "ioboard_mode").value_or(0);

            std::filesystem::path bios_path = base_path / bios;
            std::filesystem::path nand_path = base_path / nand;
            std::filesystem::path sram_path = base_path / "sram.bin";

            if (!std::filesystem::exists(bios_path)) {
                iris_error(&iris->log.emu, "Couldn't find bootrom file \"{}\"", bios_path.string().c_str());

                push_info(iris, "Couldn't start arcade game (Missing bootrom)");

                return false;
            }

            if (!std::filesystem::exists(nand_path)) {
                iris_error(&iris->log.emu, "Couldn't find NAND file \"{}\"", nand_path.string().c_str());

                push_info(iris, "Couldn't start arcade game (Missing NAND)");

                return false;
            }

            ps2::load_bios(iris->ps2, bios_path.string().c_str());
            ps2::set_system(iris->ps2, system);
            s14x::nand::load(iris->ps2->s14x_nand, nand_path.string().c_str());
            s14x::sram::load(iris->ps2->s14x_sram, sram_path.string().c_str());

            if (iris->ps2->s14x_ioboard) {
                iris->ps2->s14x_ioboard->mode = ioboard_mode;
            }

            ps2::reset(iris->ps2);

            iris->loaded = name + " (" + id + ")";

            if (iris->autostart) {
                iris->debug.pause = false;
            }

            return true;
        } break;

        default: {
            const char* names[] = {
                "Auto",
                "Retail (Fat)",
                "Retail (Slim)",
                "PSX DESR",
                "TEST unit (DTL-H)",
                "TOOL unit (DTL-T)",
                "Konami Python",
                "Konami Python 2",
                "Namco System 147",
                "Namco System 148",
                "Namco System 246",
                "Namco System 256"
            };

            iris_error(&iris->log.emu, "{} isn't supported yet", names[system]);
        } break;
    }

    return false;
}

static int memory_card_type(const char* path) {
    FILE* file = fopen(path, "rb");

    if (!file)
        return fs::mkfs::MKFS_PS2_MCD;

    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fclose(file);

    return size < 0x800000 ? fs::mkfs::MKFS_PS1_MCD : fs::mkfs::MKFS_PS2_MCD;
}

int format_memory_card(Instance* iris, int slot) {
    const std::string& path = slot ? iris->paths.mcd1_path : iris->paths.mcd0_path;

    if (path.empty())
        return 0;

    bool attached = iris->input.mcd_slot_type[slot];

    if (attached)
        detach_memory_card(iris, slot);

    fs::mkfs::Params params;

    params.type = memory_card_type(path.c_str());

    int r = fs::mkfs::format(iris->logger, path.c_str(), params);

    if (attached)
        attach_memory_card(iris, slot, path.c_str());

    return r == fs::FS_OK;
}

int attach_memory_card(Instance* iris, int slot, const char* path) {
    detach_memory_card(iris, slot);

    FILE* file = fopen(path, "rb");

    if (!file) {
        return 0;
    }

    fseek(file, 0, SEEK_END);
    int size = ftell(file);
    fclose(file);

    if (size < 0x800000) {
        dev::ps1_mcd::Ps1Mcd* mcd = dev::ps1_mcd::attach(iris->logger, iris->ps2->sio2, slot+2, path);

        std::string ext = get_extension(path);

        if (ext == "psm" || ext == "pocket") {
            dev::ps1_mcd::set_type(mcd, 1);

            iris->input.mcd_slot_type[slot] = 3;
        } else {
            dev::ps1_mcd::set_type(mcd, 0);

            iris->input.mcd_slot_type[slot] = 2;
        }

        return 1;
    }

    dev::mcd::attach(iris->logger, iris->ps2->sio2, slot+2, path);

    iris->input.mcd_slot_type[slot] = 1;

    return 1;
}

void detach_memory_card(Instance* iris, int slot) {
    iris->input.mcd_slot_type[slot] = 0;

    sio2::detach_device(iris->ps2->sio2, slot+2);
}

const char* g_system_names[] = {
    "Auto",
    "PlayStation 2 (Fat)",
    "PlayStation 2 (Slim)",
    "PSX DESR",
    "TEST Unit",
    "TOOL Unit",
    "Konami Python",
    "Konami Python 2",
    "Namco System 147",
    "Namco System 148",
    "Namco System 246",
    "Namco System 256"
};

const char* get_system_name(Instance* iris, int system) {
    return g_system_names[system];
}

const char* get_current_system_name(Instance* iris) {
    switch (iris->system) {
        case ps2::AUTO: return get_system_name(iris, iris->ps2->detected_system);
        case ps2::RETAIL:
        case ps2::RETAIL_DRAGON:
        case ps2::PSX_DESR:
        case ps2::TEST:
        case ps2::TOOL:
        case ps2::KONAMI_PYTHON:
        case ps2::KONAMI_PYTHON2:
        case ps2::NAMCO_SYSTEM_147:
        case ps2::NAMCO_SYSTEM_148:
        case ps2::NAMCO_SYSTEM_246:
        case ps2::NAMCO_SYSTEM_256:
            return g_system_names[iris->system];
        default: return "Unknown";
    }
}

int get_system_count(Instance* iris) {
    return sizeof(g_system_names) / sizeof(const char*);
}

std::string& strtolower(std::string& str) {
    std::transform(str.begin(), str.end(), str.begin(), [](unsigned char c){ return std::tolower(c); });

    return str;
}

std::string& strtoupper(std::string& str) {
    std::transform(str.begin(), str.end(), str.begin(), [](unsigned char c){ return std::toupper(c); });

    return str;
}

std::filesystem::path get_rom_path(std::filesystem::path filename, std::string ext) {
    std::filesystem::path path_lc = filename;
    std::filesystem::path path_uc = filename;

    path_lc += "." + strtolower(ext);
    path_uc += "." + strtoupper(ext);

    if (std::filesystem::exists(path_lc)) {
        return path_lc;
    } else if (std::filesystem::exists(path_uc)) {
        return path_uc;
    }

    return "";
}

bool load_rom_files(Instance* iris) {
    bool loaded = ps2::load_bios(iris->ps2, iris->paths.bios_path.c_str());

    std::string rom1 = iris->paths.rom1_path;
    std::string rom2 = iris->paths.rom2_path;
    std::string nvram = iris->paths.nvram_path;

    if (iris->paths.auto_paths) {
        std::filesystem::path bios_path(iris->paths.bios_path);

        // Get full path without extension
        bios_path = bios_path.parent_path() / bios_path.stem();

        if (!iris->paths.pinned_rom1)
            rom1 = get_rom_path(bios_path, "rom1").string();

        if (!iris->paths.pinned_rom2)
            rom2 = get_rom_path(bios_path, "rom2").string();

        if (!iris->paths.pinned_nvram)
            nvram = get_rom_path(bios_path, "nvm").string();
    }

    if (rom1.size()) {
        ps2::load_rom1(iris->ps2, rom1.c_str());
    }

    if (rom2.size()) {
        ps2::load_rom2(iris->ps2, rom2.c_str());
    }

    if (nvram.size()) {
        cdvd::load_nvram(iris->ps2->cdvd, nvram.c_str());
    }

    return loaded;
}

}