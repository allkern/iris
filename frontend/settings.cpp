#include "iris.hpp"
#include "config.hpp"

#include "ps2_elf.h"
#include "ps2_iso9660.h"

#define TOML_EXCEPTIONS 0
#include <toml++/toml.hpp>

namespace iris {

void print_version(iris::instance* iris) {
    puts(
        "iris (" STR(_IRIS_VERSION) "-" STR(_IRIS_COMMIT) " " STR(_IRIS_OSVERSION) ")\n"
        "Copyright (C) 2025 Allkern/Lisandro Alarcon\n\n"
        "MIT License\n"
        "THE SOFTWARE IS PROVIDED \"AS IS\", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR\n"
        "IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,\n"
        "FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE\n"
        "AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER\n"
        "LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,\n"
        "OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE\n"
        "SOFTWARE."
    );

    exit(0);
}

void print_help(iris::instance* iris) {
    puts(
        "Usage: iris [OPTION]... <path-to-disc-image>\n"
        "\n"
        "  -b, --bios               Specify a PlayStation 2 BIOS dump file\n"
        "  -d, --boot               Specify a direct kernel boot path\n"
        "  -i, --disc               Specify a path to a disc image file\n"
        "  -x, --executable         Specify a path to an ELF executable to be\n"
        "                             loaded on system startup\n"
        "  -h, --help               Display this help and exit\n"
        "  -v, --version            Output version information and exit\n"
        "\n"
        "Please report any bugs to <https://github.com/allkern/eegs/issues>"
    );

    exit(0);
}

int parse_toml_settings(iris::instance* iris) {
    iris->settings_path = iris->pref_path + "settings.toml";

    toml::parse_result result = toml::parse_file(iris->settings_path);

    if (!result) {
        std::string desc(result.error().description());

        printf("iris: Couldn't parse settings file: %s\n", desc.c_str());

        return 1;
    }

    toml::table& tbl = result.table();

    auto paths = tbl["paths"];
    iris->bios_path = paths["bios_path"].value_or("");
    iris->rom1_path = paths["rom1_path"].value_or("");
    iris->rom2_path = paths["rom2_path"].value_or("");

    auto window = tbl["window"];
    iris->window_width = window["window_width"].value_or(960);
    iris->window_height = window["window_height"].value_or(720);
    iris->fullscreen = window["fullscreen"].value_or(false);
    // iris->fullscreen_mode = tbl["fullscreen_mode"].value_or(0);

    auto display = tbl["display"];
    iris->aspect_mode = display["aspect_mode"].value_or(RENDERER_ASPECT_AUTO);
    iris->bilinear = display["bilinear"].value_or(true);
    iris->integer_scaling = display["integer_scaling"].value_or(false);
    iris->scale = display["scale"].value_or(1.5f);
    iris->renderer_backend = display["renderer"].value_or(RENDERER_SOFTWARE_THREAD);

    auto debugger = tbl["debugger"];
    iris->show_ee_control = debugger["show_ee_control"].value_or(false);
    iris->show_ee_state = debugger["show_ee_state"].value_or(false);
    iris->show_ee_logs = debugger["show_ee_logs"].value_or(false);
    iris->show_ee_interrupts = debugger["show_ee_interrupts"].value_or(false);
    iris->show_ee_dmac = debugger["show_ee_dmac"].value_or(false);
    iris->show_iop_control = debugger["show_iop_control"].value_or(false);
    iris->show_iop_state = debugger["show_iop_state"].value_or(false);
    iris->show_iop_logs = debugger["show_iop_logs"].value_or(false);
    iris->show_iop_interrupts = debugger["show_iop_interrupts"].value_or(false);
    iris->show_iop_modules = debugger["show_iop_modules"].value_or(false);
    iris->show_iop_dma = debugger["show_iop_dma"].value_or(false);
    iris->show_gs_debugger = debugger["show_gs_debugger"].value_or(false);
    iris->show_spu2_debugger = debugger["show_spu2_debugger"].value_or(false);
    iris->show_memory_viewer = debugger["show_memory_viewer"].value_or(false);
    iris->show_status_bar = debugger["show_status_bar"].value_or(true);
    iris->show_breakpoints = debugger["show_breakpoints"].value_or(false);
    iris->show_imgui_demo = debugger["show_imgui_demo"].value_or(false);

    toml::array* recents = tbl["recents"]["array"].as_array();

    for (int i = 0; i < recents->size(); i++)
        iris->recents.push_back(recents->at(i).as_string()->get());

    renderer_set_aspect_mode(iris->ctx, iris->aspect_mode);
    renderer_set_bilinear(iris->ctx, iris->bilinear);
    renderer_set_integer_scaling(iris->ctx, iris->integer_scaling);
    renderer_set_scale(iris->ctx, iris->scale);

    return 0;
}

void cli_check_for_help_version(iris::instance* iris, int argc, const char* argv[]) {
    for (int i = 1; i < argc; i++) {
        std::string a(argv[i]);

        if (a == "-h" || a == "--help") {
            print_help(iris);
        } else if (a == "-v" || a == "--version") {
            print_version(iris);
        }
    }
}

void parse_cli_settings(iris::instance* iris, int argc, const char* argv[]) {
    std::string bios_path;
    std::string rom1_path;
    std::string rom2_path;

    for (int i = 1; i < argc; i++) {
        std::string a(argv[i]);

        if (a == "-x" || a == "--executable") {
            iris->elf_path = argv[i+1];

            ++i;
        } else if (a == "-d" || a == "--boot") {
            iris->boot_path = argv[i+1];

            ++i;
        } else if (a == "-b" || a == "--bios") {
            bios_path = argv[i+1];

            ++i;
        } else if (a == "--rom1") {
            rom1_path = argv[i+1];

            ++i;
        } else if (a == "--rom2") {
            rom2_path = argv[i+1];

            ++i;
        } else if (a == "-i" || a == "--disc") {
            iris->disc_path = argv[i+1];

            ++i;
        } else {
            iris->disc_path = argv[i];
        }
    }

    if (bios_path.size()) {
        ps2_load_bios(iris->ps2, bios_path.c_str());
    } else {
        if (iris->bios_path.size()) {
            ps2_load_bios(iris->ps2, iris->bios_path.c_str());
        } else {
            iris->show_bios_setting_window = true;
        }
    }

    if (rom1_path.size()) {
        ps2_load_rom1(iris->ps2, rom1_path.c_str());
    } else {
        if (iris->rom1_path.size()) {
            ps2_load_rom1(iris->ps2, iris->rom1_path.c_str());
        }
    }

    if (rom2_path.size()) {
        ps2_load_rom2(iris->ps2, rom2_path.c_str());
    } else {
        if (iris->rom2_path.size()) {
            ps2_load_rom2(iris->ps2, iris->rom2_path.c_str());
        }
    }

    if (iris->elf_path.size()) {
        ps2_elf_load(iris->ps2, iris->elf_path.c_str());

        iris->loaded = iris->elf_path;
    }

    if (iris->boot_path.size()) {
        ps2_boot_file(iris->ps2, iris->boot_path.c_str());

        iris->loaded = iris->boot_path;
    }

    if (iris->disc_path.size()) {
        if (ps2_cdvd_open(iris->ps2->cdvd, iris->disc_path.c_str()))
            return;

        char* boot_file = disc_get_boot_path(iris->ps2->cdvd->disc);

        if (!boot_file)
            return;

        ps2_boot_file(iris->ps2, boot_file);

        iris->loaded = iris->disc_path;
    }
}

int init_settings(iris::instance* iris, int argc, const char* argv[]) {
    int r = parse_toml_settings(iris);
    parse_cli_settings(iris, argc, argv);

    return r;
}

void close_settings(iris::instance* iris) {
    if (!iris->dump_to_file)
        return;

    std::ofstream file(iris->settings_path);

    file << "# File auto-generated by Iris 0.1\n\n";

    auto tbl = toml::table {
        { "debugger", toml::table {
            { "show_ee_control", iris->show_ee_control },
            { "show_ee_state", iris->show_ee_state },
            { "show_ee_logs", iris->show_ee_logs },
            { "show_ee_interrupts", iris->show_ee_interrupts },
            { "show_ee_dmac", iris->show_ee_dmac },
            { "show_iop_control", iris->show_iop_control },
            { "show_iop_state", iris->show_iop_state },
            { "show_iop_logs", iris->show_iop_logs },
            { "show_iop_interrupts", iris->show_iop_interrupts },
            { "show_iop_modules", iris->show_iop_modules},
            { "show_iop_dma", iris->show_iop_dma },
            { "show_gs_debugger", iris->show_gs_debugger },
            { "show_spu2_debugger", iris->show_spu2_debugger },
            { "show_memory_viewer", iris->show_memory_viewer },
            { "show_status_bar", iris->show_status_bar },
            { "show_breakpoints", iris->show_breakpoints },
            { "show_imgui_demo", iris->show_imgui_demo }
        } },
        { "display", toml::table {
            { "scale", iris->scale },
            { "aspect_mode", iris->aspect_mode },
            { "integer_scaling", iris->integer_scaling },
            { "bilinear", iris->bilinear },
            { "renderer", iris->renderer_backend }
        } },
        { "paths", toml::table {
            { "bios_path", iris->bios_path },
            { "rom1_path", iris->rom1_path },
            { "rom2_path", iris->rom2_path },
        } },
        { "recents", toml::table {
            { "array", toml::array() }
        } }
    };

    toml::array* recents = tbl["recents"]["array"].as_array();

    for (const std::string& s : iris->recents)
        recents->push_back(s);

    file << tbl;
}

}
