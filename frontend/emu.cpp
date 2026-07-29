#include "iris.hpp"
#include "arcade.hpp"
#include "slirp.hpp"

#include "miniz.h"

#include <filesystem>
#include <algorithm>
#include <optional>
#include <cctype>
#include <thread>

namespace iris::emu {

bool init(iris::instance* iris) {
    // Initialize our emulator state
    iris->ps2 = ps2_create();

    ps2_init(iris->ps2);
    ps2_init_tty_handler(iris->ps2, PS2_TTY_EE, iris::handle_ee_tty_event, iris);
    ps2_init_tty_handler(iris->ps2, PS2_TTY_IOP, iris::handle_iop_tty_event, iris);
    ps2_init_tty_handler(iris->ps2, PS2_TTY_SYSMEM, iris::handle_sysmem_tty_event, iris);

    iris->ds[0] = ds_attach(iris->ps2->sio2, 0);

    return true;
}

void destroy(iris::instance* iris) {
    iris::slirp::stop();

    if (iris->ps2) ps2_destroy(iris->ps2);
}

const char* get_extension(const char* path) {
    const char* dot = strrchr(path, '.');

    if (!dot || dot == path)
        return nullptr;

    return dot + 1;
}

static void finish_load(iris::instance* iris, int result, std::string name = "") {
    iris->load_result = result;
    iris->load_pending_name = std::move(name);
    iris->load_ready.store(true, std::memory_order_release);
}

void finalize_load(iris::instance* iris) {
    vulkan::wait_idle(iris);

    renderer_hotswap(iris->renderer, iris->renderer_backend);

    iris->loading_file_active = false;
    iris->loading_target = "";
    iris->show_gamelist = false;

    imgui::end_dim(iris);

    if (iris->load_result == 0) {
        renderer_reset(iris->renderer);

        iris->image = {};

        iris->loaded = iris->load_pending_name;

        if (iris->autostart)
            iris->pause = false;
    }
}

int open_archive(iris::instance* iris, std::string path) {
    mz_zip_archive zip;

    mz_zip_zero_struct(&zip);

    if (!mz_zip_reader_init_file(&zip, path.c_str(), 0)) {
        printf("emu: Couldn't open archive \"%s\"\n", path.c_str());

        return 1;
    }

    // Decompress everything into pref_path/tmp/
    std::filesystem::path tmp_path = std::filesystem::path(iris->pref_path) / "tmp";

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
            printf("emu: Failed to extract \"%s\" from archive\n", stat.m_filename);
        }
    }

    mz_zip_reader_end(&zip);

    return 0;
}

int open_file_thread(iris::instance* iris, std::string file) {
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
    if (ext == ".iso" || ext == ".bin" || ext == ".cue" ||
        ext == ".chd" || ext == ".cso" || ext == ".zso") {
        if (ps2_cdvd_open(iris->ps2->cdvd, file.c_str(), 0)) {
            finish_load(iris, 1);

            return 1;
        }

        char* boot_file = disc_get_boot_path(iris->ps2->cdvd->disc);

        if (!boot_file) {
            finish_load(iris, 2);

            return 2;
        }

        elf::load_symbols_from_disc(iris);

        ps2_set_system(iris->ps2, iris->system);
        emu::load_rom_files(iris);
        ps2_boot_file(iris->ps2, boot_file);

        finish_load(iris, 0, file);

        return 0;
    }

    elf::load_symbols_from_file(iris, file);

    iris->host_elf_dir = std::filesystem::path(file).parent_path().string();

    if (iris->host_from_elf)
        settings::apply_device_maps(iris);

    // Note: We need the trailing whitespaces here because of IOMAN HLE
    // Load executable
    file = "host:  " + file;

    ps2_set_system(iris->ps2, iris->system);
    emu::load_rom_files(iris);
    ps2_boot_file(iris->ps2, file.c_str());

    finish_load(iris, 0, file);

    return 0;
}

int open_file(iris::instance* iris, std::string file) {
    std::filesystem::path path(file);

    iris->loading_target = path.filename().string();
    iris->loading_file_active = true;
    iris->load_ready = false;
    iris->pause = true;

    renderer_hotswap(iris->renderer, RENDERER_BACKEND_NULL);

    imgui::start_dim(iris, 0.35f, 100);

    iris->load_pending_file = file;
    iris->load_start_pending = true;

    return 0;
}

void start_pending_load(iris::instance* iris) {
    if (!iris->load_start_pending)
        return;

    iris->load_start_pending = false;

    std::thread t(open_file_thread, iris, iris->load_pending_file);

    t.detach();
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

bool load_arcade(iris::instance* iris, std::string path) {
    std::filesystem::path base_path(path);

    std::string id = base_path.stem().string();
    std::string name = query_arcade_value<std::string>(id, "name").value_or("");

    if (!name.size()) {
        return false;
    }

    printf("emu: Loading arcade game \"%s\"...\n", id.c_str(), name.c_str());

    int system = query_arcade_value<int>(id, "system").value_or(PS2_SYSTEM_AUTO);

    switch (system) {
        case PS2_SYSTEM_NAMCO_S147:
        case PS2_SYSTEM_NAMCO_S148: {
            std::string bios = query_arcade_value<std::string>(id, "bios").value_or("");
            std::string nand = query_arcade_value<std::string>(id, "nand").value_or("");

            int ioboard_mode = query_arcade_value<int>(id, "ioboard_mode").value_or(0);

            std::filesystem::path bios_path = base_path / bios;
            std::filesystem::path nand_path = base_path / nand;
            std::filesystem::path sram_path = base_path / "sram.bin";

            if (!std::filesystem::exists(bios_path)) {
                printf("emu: Couldn't find bootrom file \"%s\"\n", bios_path.string().c_str());

                push_info(iris, "Couldn't start arcade game (Missing bootrom)");

                return false;
            }

            if (!std::filesystem::exists(nand_path)) {
                printf("emu: Couldn't find NAND file \"%s\"\n", nand_path.string().c_str());

                push_info(iris, "Couldn't start arcade game (Missing NAND)");

                return false;
            }

            ps2_load_bios(iris->ps2, bios_path.string().c_str());
            ps2_set_system(iris->ps2, system);
            s14x_nand_load(iris->ps2->s14x_nand, nand_path.string().c_str());
            s14x_sram_load(iris->ps2->s14x_sram, sram_path.string().c_str());

            if (iris->ps2->s14x_ioboard) {
                iris->ps2->s14x_ioboard->mode = ioboard_mode;
            }

            ps2_reset(iris->ps2);

            iris->loaded = name + " (" + id + ")";

            if (iris->autostart) {
                iris->pause = false;
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

            printf("emu: %s isn't supported yet\n", names[system]);
        } break;
    }

    return false;
}

int attach_memory_card(iris::instance* iris, int slot, const char* path) {
    detach_memory_card(iris, slot);

    FILE* file = fopen(path, "rb");

    if (!file) {
        return 0;
    }

    fseek(file, 0, SEEK_END);
    int size = ftell(file);
    fclose(file);

    if (size < 0x800000) {
        struct ps1_mcd_state* mcd = ps1_mcd_attach(iris->ps2->sio2, slot+2, path);

        std::string ext = get_extension(path);

        if (ext == "psm" || ext == "pocket") {
            ps1_mcd_set_type(mcd, 1);

            iris->mcd_slot_type[slot] = 3;
        } else {
            ps1_mcd_set_type(mcd, 0);

            iris->mcd_slot_type[slot] = 2;
        }

        return 1;
    }

    mcd_attach(iris->ps2->sio2, slot+2, path);

    iris->mcd_slot_type[slot] = 1;

    return 1;
}

void detach_memory_card(iris::instance* iris, int slot) {
    iris->mcd_slot_type[slot] = 0;

    ps2_sio2_detach_device(iris->ps2->sio2, slot+2);
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

const char* get_system_name(iris::instance* iris, int system) {
    return g_system_names[system];
}

const char* get_current_system_name(iris::instance* iris) {
    switch (iris->system) {
        case PS2_SYSTEM_AUTO: return get_system_name(iris, iris->ps2->detected_system);
        case PS2_SYSTEM_RETAIL:
        case PS2_SYSTEM_RETAIL_DECKARD:
        case PS2_SYSTEM_DESR:
        case PS2_SYSTEM_TEST:
        case PS2_SYSTEM_TOOL:
        case PS2_SYSTEM_KONAMI_PYTHON:
        case PS2_SYSTEM_KONAMI_PYTHON2:
        case PS2_SYSTEM_NAMCO_S147:
        case PS2_SYSTEM_NAMCO_S148:
        case PS2_SYSTEM_NAMCO_S246:
        case PS2_SYSTEM_NAMCO_S256:
            return g_system_names[iris->system];
        default: return "Unknown";
    }
}

int get_system_count(iris::instance* iris) {
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

bool load_rom_files(iris::instance* iris) {
    ps2_load_bios(iris->ps2, iris->bios_path.c_str());

    if (iris->auto_paths) {
        std::filesystem::path bios_path(iris->bios_path);

        // Get full path without extension
        bios_path = bios_path.parent_path() / bios_path.stem();

        std::filesystem::path rom1_path = get_rom_path(bios_path, "rom1");
        std::filesystem::path rom2_path = get_rom_path(bios_path, "rom2");
        std::filesystem::path nvm_path = get_rom_path(bios_path, "nvm");

        if (rom1_path.string().size()) {
            ps2_load_rom1(iris->ps2, rom1_path.string().c_str());
        }

        if (rom2_path.string().size()) {
            ps2_load_rom2(iris->ps2, rom2_path.string().c_str());
        }

        if (nvm_path.string().size()) {
            ps2_cdvd_load_nvram(iris->ps2->cdvd, nvm_path.string().c_str());
        }

        return true;
    }

    if (iris->rom1_path.size()) {
        ps2_load_rom1(iris->ps2, iris->rom1_path.c_str());
    }

    if (iris->rom2_path.size()) {
        ps2_load_rom2(iris->ps2, iris->rom2_path.c_str());
    }

    if (iris->nvram_path.size()) {
        ps2_cdvd_load_nvram(iris->ps2->cdvd, iris->nvram_path.c_str());
    }

    return true;
}

}