#include "iris.hpp"
#include "arcade.hpp"

#include <filesystem>
#include <optional>

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
    if (iris->ps2) ps2_destroy(iris->ps2);
}

const char* get_extension(const char* path) {
    const char* dot = strrchr(path, '.');

    if (!dot || dot == path)
        return nullptr;

    return dot + 1;
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

            ps2_set_system(iris->ps2, system);
            ps2_load_bios(iris->ps2, bios_path.string().c_str());
            s14x_nand_load(iris->ps2->s14x_nand, nand_path.string().c_str());
            s14x_sram_load(iris->ps2->s14x_sram, sram_path.string().c_str());
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

}