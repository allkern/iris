#include "iris.hpp"
#include "arcade.hpp"
#include "archive.hpp"
#include "ini.hpp"
#include "slirp.hpp"

#include "ps2.hpp"
#include "ps2_elf.hpp"
#include "fs/mkfs.hpp"
#include "fs/fs.hpp"
#include "fs/blk.hpp"
#include "shared/ata/disc.hpp"
#include "kp2/p2io.hpp"
#include "kp1/p1io.hpp"
#include "iop/mg.hpp"

#include <filesystem>
#include <vector>
#include <algorithm>
#include <optional>
#include <cctype>
#include <thread>

namespace iris::emu {

struct ArcadeBiosSlot {
    int system;
    const char* key;
    const char* label;
};

static const ArcadeBiosSlot g_arcade_bios_slots[ARCADE_BIOS_COUNT] = {
    { ps2::NAMCO_SYSTEM_147, "arcade_bios_147_path", "Namco System 147" },
    { ps2::NAMCO_SYSTEM_148, "arcade_bios_148_path", "Namco System 148" },
    { ps2::NAMCO_SYSTEM_246, "arcade_bios_246_path", "Namco System 246" },
    { ps2::NAMCO_SYSTEM_256, "arcade_bios_256_path", "Namco System 256/Super 256" },
    { ps2::KONAMI_PYTHON, "arcade_bios_python_path", "Konami Python" },
    { ps2::KONAMI_PYTHON2, "arcade_bios_python2_path", "Konami Python 2" }
};

const char* get_arcade_bios_label(int slot) {
    if (slot < 0 || slot >= ARCADE_BIOS_COUNT)
        return "Unknown";

    return g_arcade_bios_slots[slot].label;
}

const char* get_arcade_bios_key(int slot) {
    if (slot < 0 || slot >= ARCADE_BIOS_COUNT)
        return "";

    return g_arcade_bios_slots[slot].key;
}

int get_arcade_bios_slot(int system) {
    if (system == ps2::NAMCO_SYSTEM_SUPER_256)
        system = ps2::NAMCO_SYSTEM_256;

    for (int i = 0; i < ARCADE_BIOS_COUNT; i++)
        if (g_arcade_bios_slots[i].system == system)
            return i;

    return -1;
}

std::string get_arcade_bios_path(Instance* iris, int system) {
    int slot = get_arcade_bios_slot(system);

    if (slot < 0)
        return "";

    return iris->paths.arcade_bios_paths[slot];
}

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

    clean_arcade_files(iris);
    clean_tmp_files(iris);
}

void clean_arcade_files(Instance* iris) {
    if (iris->cache_arcade_files)
        return;

    std::error_code ec;

    std::filesystem::remove_all(std::filesystem::path(iris->paths.pref_path) / "arcade", ec);
}

void clean_tmp_files(Instance* iris) {
    std::error_code ec;

    std::filesystem::remove_all(std::filesystem::path(iris->paths.pref_path) / "tmp", ec);
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

        if (iris->arcade_id.empty()) {
            clean_arcade_files(iris);
        }

        if (iris->autostart)
            iris->debug.pause = false;
    }
}

static bool paths_equal(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) {
        return false;
    }

    for (size_t i = 0; i < a.size(); i++) {
        if (tolower(a[i]) != tolower(b[i])) {
            return false;
        }
    }

    return true;
}

static bool has_extension(const std::string& path, const std::string& extension) {
    if (path.size() < extension.size()) {
        return false;
    }

    return paths_equal(path.substr(path.size() - extension.size()), extension);
}

static std::string replace_extension(const std::string& path, const std::string& extension) {
    size_t dot = path.find_last_of('.');

    if (dot == std::string::npos) {
        return path + extension;
    }

    return path.substr(0, dot) + extension;
}

static std::string find_archive_path(const std::vector <archive::Entry>& entries, const std::string& path) {
    for (const archive::Entry& entry : entries) {
        if (paths_equal(entry.path, path)) {
            return entry.path;
        }
    }

    return "";
}

static std::string find_disc_image(const std::vector <archive::Entry>& entries) {
    for (const archive::Entry& entry : entries) {
        if (entry.directory) {
            continue;
        }

        if (!is_disc_image(entry.path)) {
            continue;
        }

        if (!has_extension(entry.path, ".bin")) {
            return entry.path;
        }

        std::string cue = find_archive_path(entries, replace_extension(entry.path, ".cue"));

        if (cue.size()) {
            return cue;
        }

        return entry.path;
    }

    return "";
}

std::string open_archive(Instance* iris, std::string path) {
    std::vector <archive::Entry> entries;

    if (!archive::list(path, &entries)) {
        iris_error(&iris->log.emu, "Couldn't read archive \"{}\"", path.c_str());

        push_info(iris, "Couldn't open archive (Unreadable)");

        return "";
    }

    std::string name = find_disc_image(entries);

    if (name.empty()) {
        iris_error(&iris->log.emu, "Archive \"{}\" doesn't hold a disc image", path.c_str());

        push_info(iris, "Couldn't open archive (No disc image inside)");

        return "";
    }

    std::filesystem::path extract_path =
        std::filesystem::path(iris->paths.pref_path) / "tmp" / std::filesystem::path(path).stem();

    iris_info(&iris->log.emu, "Extracting \"{}\" from \"{}\"...", name.c_str(), path.c_str());

    if (!archive::extract_all(path, extract_path)) {
        iris_error(&iris->log.emu, "Couldn't extract archive \"{}\"", path.c_str());

        push_info(iris, "Couldn't open archive (Extraction failed)");

        return "";
    }

    return (extract_path / name).string();
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

    iris->arcade_id = "";

    std::string display_path = file;

    if (archive::is_archive(path)) {
        cdvd::close(iris->ps2->cdvd);

        clean_tmp_files(iris);

        file = open_archive(iris, file);

        if (file.empty()) {
            finish_load(iris, 1);

            return 1;
        }
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

        finish_load(iris, 0, display_path);

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

    if (is_arcade_file(iris, file)) {
        return load_arcade(iris, file) ? 0 : 1;
    }

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
    iris->arcade_id = "";

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

static int boot_arcade_thread(Instance* iris, std::string path);

std::filesystem::path get_rom_path(std::filesystem::path filename, std::string ext);

struct ArcadeFiles {
    std::filesystem::path bios;
    std::filesystem::path nand;
    std::filesystem::path dongle;
    std::filesystem::path media;
    std::filesystem::path loader;
    std::filesystem::path sram;
    std::filesystem::path nvram;
    std::filesystem::path hdd_id;
    std::filesystem::path dongle_black;
    std::filesystem::path dongle_white;
    std::filesystem::path io_bootrom;
    std::filesystem::path io_configrom;
    std::filesystem::path dongle_int;
    std::filesystem::path dongle_ext;
    std::filesystem::path bbsram;
    std::filesystem::path mcd_id;
    std::filesystem::path bbsram_seed;

    int media_type;
};

void start_pending_load(Instance* iris) {
    if (!iris->load_start_pending)
        return;

    iris->load_start_pending = false;

    if (iris->load_pending_boot) {
        iris->load_pending_boot = false;

        std::thread(boot_ps2_path_thread, iris, iris->load_pending_file).detach();

        return;
    }

    if (iris->load_pending_arcade) {
        iris->load_pending_arcade = false;

        std::thread(boot_arcade_thread, iris, iris->load_pending_file).detach();

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

struct ArchiveIndex {
    std::filesystem::path path;
    std::vector<archive::Entry> entries;
};

struct ArcadeFileNames {
    std::vector <std::string> bios;
    std::vector <std::string> nand;
    std::vector <std::string> dongle;
    std::vector <std::string> media;
    std::vector <std::string> loader;
    std::vector <std::string> hdd_id;
    std::vector <std::string> nvram;
    std::vector <std::string> dongle_black;
    std::vector <std::string> dongle_white;
    std::vector <std::string> io_bootrom;
    std::vector <std::string> io_configrom;
    std::vector <std::string> dongle_int;
    std::vector <std::string> dongle_ext;
    std::vector <std::string> bbsram;
    std::vector <std::string> mcd_id;
};

struct ArcadeSource {
    std::string id;
    std::string name;
    std::string bootprog;

    int system = ps2::AUTO;
    int media_type = s2x6::acata::MEDIA_DVD;
    int uart_device = s2x6::acuart::DEVICE_NONE;
    int jvs_mode = s2x6::acjv::MODE_DEFAULT;
    int wheel_style = s2x6::acjv::WHEEL_STANDARD;
    int gun_trigger = s2x6::acjv::GUN_DEFAULT_TRIGGER;
    int gun_pedal = s2x6::acjv::GUN_DEFAULT_PEDAL;
    int gun_board = s2x6::acjv::GUN_BOARD_CLASSIC;
    int gun_sensor = 0;
    int gun_sensor_active_high = 0;
    int ioboard_mode = 0;
    int input_type = kp2::p2io::INPUT_THRILL_DRIVE;
    int io_mode = kp1::p1io::IO_MODE_JVS;
    int media_hdd = 0;
    int dip_switches = 0;
    int force_31khz = 0;
    bool needs_white_dongle = false;

    ArcadeFileNames names;

    std::filesystem::path extract_path;
    std::vector<std::filesystem::path> search_paths;
    std::vector<ArchiveIndex> archives;
};

static std::string lowercase(std::string text) {
    for (char& c : text)
        c = tolower(c);

    return text;
}

static std::vector <std::string> query_arcade_names(const std::string& id, const std::string& key, const std::string& fallback) {
    std::vector <std::string> names;

    auto it = g_arcade_definitions.find(id);

    if (it != g_arcade_definitions.end()) {
        const toml::table* table = it->second.as_table();
        const toml::node* node = table ? table->get(key) : nullptr;

        if (const toml::array* listed = node ? node->as_array() : nullptr) {
            for (const toml::node& element : *listed) {
                if (auto value = element.value<std::string>()) {
                    names.push_back(*value);
                }
            }
        } else if (node) {
            if (auto value = node->value<std::string>()) {
                names.push_back(*value);
            }
        }
    }

    if (names.empty() && fallback.size()) {
        names.push_back(fallback);
    }

    return names;
}

static ArcadeFileNames arcade_file_names(const std::string& id) {
    std::string gameid = query_arcade_value<std::string>(id, "gameid").value_or(id);

    ArcadeFileNames names;

    names.bios = query_arcade_names(id, "bios", "bios.bin");
    names.nand = query_arcade_names(id, "nand", "nand.bin");
    names.dongle = query_arcade_names(id, "dongle", gameid + ".ps2");
    names.media = query_arcade_names(id, "media", gameid + ".chd");
    names.loader = query_arcade_names(id, "loader", "boot.elf");
    names.hdd_id = query_arcade_names(id, "hdd_id", "ps2_hdd_id.bin");
    names.nvram = query_arcade_names(id, "nvram", "ps2_nvram.bin");
    names.dongle_black = query_arcade_names(id, "dongle_black", "ds2430_black.bin");
    names.dongle_white = query_arcade_names(id, "dongle_white", "ds2430_white.bin");
    names.io_bootrom = query_arcade_names(id, "io_bootrom", "p1io_bootrom.bin");
    names.io_configrom = query_arcade_names(id, "io_configrom", "d72872gc.crom");
    names.dongle_int = query_arcade_names(id, "dongle_int", "ds2430_internal.bin");
    names.dongle_ext = query_arcade_names(id, "dongle_ext", "ds2430_external.bin");
    names.bbsram = query_arcade_names(id, "bbsram", "m48t58y.u48");
    names.mcd_id = query_arcade_names(id, "mcd_id", "kn00002.id");

    return names;
}

static void load_arcade_definition(ArcadeSource* source) {
    source->name = query_arcade_value<std::string>(source->id, "name").value_or("");
    source->system = query_arcade_value<int>(source->id, "system").value_or(ps2::AUTO);
    source->media_type = query_arcade_value<int>(source->id, "media_type").value_or(s2x6::acata::MEDIA_DVD);
    source->uart_device = query_arcade_value<int>(source->id, "uart_device").value_or(s2x6::acuart::DEVICE_NONE);
    source->jvs_mode = query_arcade_value<int>(source->id, "jvs_mode").value_or(s2x6::acjv::MODE_DEFAULT);
    source->wheel_style = query_arcade_value<int>(source->id, "wheel_style").value_or(s2x6::acjv::WHEEL_STANDARD);
    source->gun_trigger = query_arcade_value<int>(source->id, "gun_trigger").value_or(s2x6::acjv::GUN_DEFAULT_TRIGGER);
    source->gun_pedal = query_arcade_value<int>(source->id, "gun_pedal").value_or(s2x6::acjv::GUN_DEFAULT_PEDAL);
    source->gun_board = query_arcade_value<int>(source->id, "gun_board").value_or(s2x6::acjv::GUN_BOARD_CLASSIC);
    source->gun_sensor = query_arcade_value<int>(source->id, "gun_sensor").value_or(0);
    source->gun_sensor_active_high = query_arcade_value<int>(source->id, "gun_sensor_active_high").value_or(0);
    source->ioboard_mode = query_arcade_value<int>(source->id, "ioboard_mode").value_or(0);
    source->input_type = query_arcade_value<int>(source->id, "input_type").value_or(kp2::p2io::INPUT_THRILL_DRIVE);
    source->io_mode = query_arcade_value<int>(source->id, "io_mode").value_or(kp1::p1io::IO_MODE_JVS);
    source->media_hdd = query_arcade_value<int>(source->id, "media_hdd").value_or(0);
    source->dip_switches = query_arcade_value<int>(source->id, "dipsw").value_or(0);
    source->force_31khz = query_arcade_value<int>(source->id, "force_31khz").value_or(0);
    source->needs_white_dongle = query_arcade_value<int>(source->id, "white_dongle").value_or(0) != 0;
    source->bootprog = query_arcade_value<std::string>(source->id, "bootprog").value_or("");
    source->names = arcade_file_names(source->id);
}

static const archive::Entry* find_archive_entry(const ArchiveIndex& index, const std::string& name) {
    std::string wanted = lowercase(name);

    for (const archive::Entry& entry : index.entries) {
        if (entry.directory)
            continue;

        if (lowercase(entry.name) == wanted)
            return &entry;
    }

    return nullptr;
}

static const ArchiveIndex* find_archive_with_file(const ArcadeSource& source, const std::vector <std::string>& names) {
    for (const ArchiveIndex& index : source.archives)
        for (const std::string& name : names)
            if (find_archive_entry(index, name))
                return &index;

    return nullptr;
}

static std::filesystem::path find_extracted_file(const ArcadeSource& source, const std::vector <std::string>& names) {
    for (const std::filesystem::path& dir : source.search_paths)
        for (const std::string& name : names)
            if (std::filesystem::exists(dir / name))
                return dir / name;

    return {};
}

static constexpr uint64_t MEDIA_MIN_SIZE = 16ull * 1024 * 1024;

static bool is_media_role(const char* prefix) {
    return prefix && std::string(prefix) == "media";
}

static std::filesystem::path find_by_fingerprint(const ArcadeSource& source, const char* prefix) {
    if (!prefix)
        return {};

    std::optional <std::string> sha1 = query_arcade_value <std::string> (source.id, std::string(prefix) + "_sha1");
    std::optional <int64_t> crc = query_arcade_value <int64_t> (source.id, std::string(prefix) + "_crc");

    if (!sha1 && !crc)
        return {};

    std::error_code ec;

    for (const std::filesystem::path& dir : source.search_paths) {
        for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
            if (ec)
                break;

            if (!entry.is_regular_file(ec))
                continue;

            arcade::Fingerprint print;

            if (!arcade::probe_file(entry.path(), &print))
                continue;

            if (sha1 && print.sha1.size() && print.sha1 == *sha1)
                return entry.path();

            if (crc && print.has_crc && print.crc == (uint32_t)*crc)
                return entry.path();
        }
    }

    return {};
}

static std::filesystem::path find_media_by_kind(const ArcadeSource& source) {
    std::vector <std::filesystem::path> compressed;
    std::vector <std::filesystem::path> large;

    std::error_code ec;

    for (const std::filesystem::path& dir : source.search_paths) {
        for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
            if (ec)
                break;

            if (!entry.is_regular_file(ec))
                continue;

            arcade::Fingerprint print;

            if (!arcade::probe_file(entry.path(), &print))
                continue;

            if (print.sha1.size()) {
                compressed.push_back(entry.path());
            } else if (print.size >= MEDIA_MIN_SIZE) {
                large.push_back(entry.path());
            }
        }
    }

    if (compressed.size() == 1)
        return compressed.front();

    if (compressed.empty() && large.size() == 1)
        return large.front();

    return {};
}

static bool arcade_file_available(const ArcadeSource& source, const std::vector <std::string>& names, const char* prefix = nullptr) {
    if (!find_extracted_file(source, names).empty())
        return true;

    if (find_archive_with_file(source, names) != nullptr)
        return true;

    if (!find_by_fingerprint(source, prefix).empty())
        return true;

    return names.size() && is_media_role(prefix) && !find_media_by_kind(source).empty();
}

static std::filesystem::path locate_arcade_file(const ArcadeSource& source, const std::vector <std::string>& names, const char* prefix = nullptr) {
    std::filesystem::path found = find_extracted_file(source, names);

    if (!found.empty())
        return found;

    for (const ArchiveIndex& index : source.archives) {
        for (const std::string& name : names) {
            if (find_archive_entry(index, name)) {
                return source.extract_path / name;
            }
        }
    }

    std::filesystem::path by_content = find_by_fingerprint(source, prefix);

    if (!by_content.empty())
        return by_content;

    if (names.size() && is_media_role(prefix)) {
        std::filesystem::path by_kind = find_media_by_kind(source);

        if (!by_kind.empty())
            return by_kind;
    }

    if (names.empty()) {
        return source.search_paths.front();
    }

    return source.search_paths.front() / names.front();
}

static bool arcade_bios_available(Instance* iris, const ArcadeSource& source) {
    if (arcade_file_available(source, source.names.bios))
        return true;

    std::string configured = get_arcade_bios_path(iris, source.system);

    if (!configured.size())
        return false;

    if (std::filesystem::exists(configured))
        return true;

    iris_error(&iris->log.emu, "Configured {} board BIOS \"{}\" is missing",
        get_system_name(iris, source.system), configured.c_str());

    return false;
}

static ArcadeFiles resolve_arcade_files(Instance* iris, const ArcadeSource& source) {
    std::filesystem::path pref_path(iris->paths.pref_path);

    ArcadeFiles files;

    files.bios = locate_arcade_file(source, source.names.bios);
    files.nand = locate_arcade_file(source, source.names.nand);
    files.dongle = locate_arcade_file(source, source.names.dongle, "dongle");
    files.media = locate_arcade_file(source, source.names.media, "media");
    files.loader = locate_arcade_file(source, source.names.loader);
    files.sram = pref_path / "acsram" / (source.id + ".bin");
    files.nvram = pref_path / "acnvram" / (source.id + ".nvm");
    files.hdd_id = locate_arcade_file(source, source.names.hdd_id, "hdd_id");
    files.dongle_black = locate_arcade_file(source, source.names.dongle_black, "dongle_black");
    files.dongle_white = locate_arcade_file(source, source.names.dongle_white, "dongle_white");
    files.io_bootrom = locate_arcade_file(source, source.names.io_bootrom);
    files.io_configrom = locate_arcade_file(source, source.names.io_configrom);
    files.dongle_int = locate_arcade_file(source, source.names.dongle_int);
    files.dongle_ext = locate_arcade_file(source, source.names.dongle_ext);
    files.bbsram = pref_path / "kp1bbsram" / (source.id + ".bin");
    files.bbsram_seed = locate_arcade_file(source, source.names.bbsram);
    files.mcd_id = locate_arcade_file(source, source.names.mcd_id);

    if (source.system == ps2::KONAMI_PYTHON2) {
        files.nvram = locate_arcade_file(source, source.names.nvram, "nvram");
    }

    if (source.system == ps2::KONAMI_PYTHON) {
        std::filesystem::path supplied = locate_arcade_file(source, source.names.nvram);

        if (std::filesystem::exists(supplied))
            files.nvram = supplied;
    }

    files.media_type = source.media_type;

    if (!std::filesystem::exists(files.bios)) {
        std::string configured = get_arcade_bios_path(iris, source.system);

        if (configured.size() && std::filesystem::exists(configured))
            files.bios = configured;
    }

    return files;
}

static bool arcade_boots_from_dongle(Instance* iris, const ArcadeSource& source) {
    switch (source.system) {
        case ps2::NAMCO_SYSTEM_246:
        case ps2::NAMCO_SYSTEM_256:
        case ps2::NAMCO_SYSTEM_SUPER_256:
            break;

        default:
            return false;
    }

    if (source.bootprog.empty())
        return false;

    return iris->arcade_dongle_boot || !arcade_file_available(source, source.names.loader);
}

static const char* find_missing_arcade_file(Instance* iris, const ArcadeSource& source, bool want_loader = false) {
    if (!arcade_bios_available(iris, source))
        return "board BIOS";

    if (source.system == ps2::NAMCO_SYSTEM_147 || source.system == ps2::NAMCO_SYSTEM_148) {
        if (!arcade_file_available(source, source.names.nand))
            return "NAND";

        return nullptr;
    }

    if (source.system == ps2::KONAMI_PYTHON) {
        if (!arcade_file_available(source, source.names.media, "media")) return "CF image";
        if (!arcade_file_available(source, source.names.dongle, "dongle")) return "memory card dongle";

        return nullptr;
    }

    if (source.system == ps2::KONAMI_PYTHON2) {
        if (!arcade_file_available(source, source.names.media, "media")) return "HDD image";
        if (!arcade_file_available(source, source.names.hdd_id, "hdd_id")) return "HDD ID";
        if (!arcade_file_available(source, source.names.nvram, "nvram")) return "NVRAM";
        if (!arcade_file_available(source, source.names.dongle_black, "dongle_black")) return "black dongle";

        if (source.needs_white_dongle && !arcade_file_available(source, source.names.dongle_white, "dongle_white"))
            return "white dongle";

        return nullptr;
    }

    if (!arcade_file_available(source, source.names.dongle)) return "dongle";
    if (!arcade_file_available(source, source.names.media)) return "media image";
    if ((want_loader || !arcade_boots_from_dongle(iris, source)) &&
        !arcade_file_available(source, source.names.loader)) return "loader";

    return nullptr;
}

static bool is_namco_game_id(const std::string& text) {
    if (text.size() != 7)
        return false;

    if (toupper(text[0]) != 'N' || toupper(text[1]) != 'M')
        return false;

    for (size_t i = 2; i < text.size(); i++)
        if (!isdigit((unsigned char)text[i]))
            return false;

    return true;
}

static int score_arcade_definition(const ArchiveIndex& index, const std::string& id) {
    auto present = [&](const char* key) {
        for (const std::string& name : query_arcade_names(id, key, "")) {
            if (find_archive_entry(index, name)) {
                return true;
            }
        }

        return false;
    };

    int score = 0;

    if (present("nand")) score += 4;
    if (present("dongle")) score += 4;
    if (present("media")) score += 2;
    if (present("bios")) score += 1;

    return score;
}

static std::string find_set_by_game_id(const std::string& gameid) {
    for (auto&& [key, value] : g_arcade_definitions) {
        std::string id(key.str());

        if (lowercase(query_arcade_value<std::string>(id, "gameid").value_or("")) == lowercase(gameid))
            return id;
    }

    return "";
}

static std::string identify_by_catalog(const std::vector <std::pair <std::string, arcade::Fingerprint>>& files, const std::string& hint) {
    arcade::Candidates candidates;

    for (const auto& [name, print] : files) {
        arcade::collect_candidates(print, name, &candidates);
    }

    std::string named = arcade::resolve_set_name(hint);

    if (named.size())
        candidates[named].score += arcade::EVIDENCE_HINT;

    std::string best_id;

    int best = 0;

    for (const auto& [id, candidate] : candidates) {
        if (candidate.score <= best)
            continue;

        best = candidate.score;
        best_id = id;
    }

    return best >= arcade::EVIDENCE_ACCEPT ? best_id : "";
}

static std::string identify_arcade_directory(const std::filesystem::path& path) {
    std::error_code ec;

    std::vector <std::pair <std::string, arcade::Fingerprint>> files;

    for (const auto& entry : std::filesystem::directory_iterator(path, ec)) {
        if (ec)
            break;

        if (!entry.is_regular_file(ec))
            continue;

        arcade::Fingerprint print;

        if (!arcade::probe_file(entry.path(), &print))
            continue;

        files.push_back({ entry.path().filename().string(), print });
    }

    return identify_by_catalog(files, lowercase(path.stem().string()));
}

static std::string identify_archive_by_catalog(const ArchiveIndex& index) {
    std::vector <std::pair <std::string, arcade::Fingerprint>> files;

    for (const archive::Entry& entry : index.entries) {
        if (entry.directory)
            continue;

        arcade::Fingerprint print;

        if (!arcade::probe_size(entry.size, &print))
            continue;

        files.push_back({ std::filesystem::path(entry.name).filename().string(), print });
    }

    return identify_by_catalog(files, lowercase(index.path.stem().string()));
}

static std::string identify_by_game_id(const ArchiveIndex& index) {
    for (const archive::Entry& entry : index.entries) {
        std::string gameid = std::filesystem::path(entry.name).stem().string();

        if (!is_namco_game_id(gameid)) {
            continue;
        }

        std::string id = find_set_by_game_id(gameid);

        if (id.size()) {
            return id;
        }
    }

    return "";
}

static std::string identify_arcade_archive(const ArchiveIndex& index) {
    std::string by_catalog = identify_archive_by_catalog(index);

    if (by_catalog.size())
        return by_catalog;

    std::string by_game_id = identify_by_game_id(index);

    if (by_game_id.size())
        return by_game_id;

    std::string best_id;
    int best_score = 0;

    for (auto&& [key, value] : g_arcade_definitions) {
        std::string id(key.str());

        int score = score_arcade_definition(index, id);

        if (score > best_score) {
            best_score = score;
            best_id = id;
        }
    }

    return best_score >= 4 ? best_id : "";
}

static bool archive_completes_source(const ArcadeSource& source, const ArchiveIndex& index) {
    const ArcadeFileNames& names = source.names;

    for (const std::vector <std::string>& role : { names.bios, names.nand, names.dongle, names.media, names.loader }) {
        if (arcade_file_available(source, role))
            continue;

        for (const std::string& name : role) {
            if (find_archive_entry(index, name)) {
                return true;
            }
        }
    }

    return false;
}

static void index_sibling_archives(Instance* iris, ArcadeSource* source) {
    if (!find_missing_arcade_file(iris, *source, true))
        return;

    std::error_code ec;

    for (const auto& entry : std::filesystem::directory_iterator(source->archives.front().path.parent_path(), ec)) {
        if (!entry.is_regular_file(ec))
            continue;

        if (entry.path() == source->archives.front().path)
            continue;

        if (!archive::is_archive(entry.path()))
            continue;

        ArchiveIndex index;

        index.path = entry.path();

        if (!archive::list(index.path, &index.entries))
            continue;

        if (!archive_completes_source(*source, index))
            continue;

        iris_info(&iris->log.emu, "Taking files missing from \"{}\" out of \"{}\"",
            source->archives.front().path.filename().string().c_str(),
            index.path.filename().string().c_str());

        source->archives.push_back(std::move(index));

        if (!find_missing_arcade_file(iris, *source, true))
            return;
    }
}

static bool is_arcade_manifest(const std::filesystem::path& path) {
    return lowercase(path.extension().string()) == ".acgame";
}

static int manifest_system(const std::string& platform) {
    if (platform == "246") return ps2::NAMCO_SYSTEM_246;
    if (platform == "256") return ps2::NAMCO_SYSTEM_256;
    if (platform == "super256") return ps2::NAMCO_SYSTEM_SUPER_256;

    return ps2::AUTO;
}

static int manifest_media_type(const std::string& media) {
    if (media == "CD") return s2x6::acata::MEDIA_CD;
    if (media == "DVD") return s2x6::acata::MEDIA_DVD;
    if (media == "HDD") return s2x6::acata::MEDIA_HDD;

    return -1;
}

static std::optional<ArcadeSource> open_arcade_manifest(Instance* iris, const std::filesystem::path& path) {
    ini::File acgame;

    if (!ini::load(path, &acgame)) {
        iris_error(&iris->log.emu, "Couldn't read arcade acgame \"{}\"", path.string().c_str());

        return {};
    }

    std::string gameid = ini::value(acgame, "game", "gameid");

    if (!is_namco_game_id(gameid)) {
        iris_error(&iris->log.emu, "Arcade acgame \"{}\" has an invalid game ID \"{}\"", path.string().c_str(), gameid.c_str());

        return {};
    }

    ArcadeSource source;

    std::string set = find_set_by_game_id(gameid);

    source.id = set.size() ? set : lowercase(gameid);

    load_arcade_definition(&source);

    source.name = ini::value(acgame, "game", "name", source.name.size() ? source.name : gameid);

    int system = manifest_system(ini::value(acgame, "game", "platform"));

    if (system != ps2::AUTO)
        source.system = system;

    int media_type = manifest_media_type(ini::value(acgame, "data", "media"));

    if (media_type >= 0)
        source.media_type = media_type;

    if (ini::value(acgame, "data", "jvsmode") == "racing")
        source.jvs_mode = s2x6::acjv::MODE_DRIVE;

    source.names.dongle = { ini::value(acgame, "data", "dongle", gameid + ".ps2") };
    source.names.media = { ini::value(acgame, "data", "mediasrc", gameid + ".chd") };
    source.names.loader = { ini::value(acgame, "data", "elf", "boot.elf") };
    source.bootprog = ini::value(acgame, "data", "bootprog", source.bootprog);

    for (const char* key : { "args", "256Region", "card", "sram" }) {
        if (ini::value(acgame, "data", key).size())
            iris_info(&iris->log.emu, "Ignoring unsupported \"{}\" key in \"{}\"", key, path.filename().string().c_str());
    }

    std::filesystem::path base = path.parent_path() / ini::value(acgame, "data", "subdir", gameid);

    source.extract_path = base;

    source.search_paths.push_back(base);
    source.search_paths.push_back(path.parent_path());

    return source;
}

static std::optional<ArcadeSource> open_arcade_source(Instance* iris, const std::filesystem::path& path) {
    ArcadeSource source;

    if (std::filesystem::is_directory(path)) {
        source.id = identify_arcade_directory(path);

        load_arcade_definition(&source);

        if (!source.name.size())
            return {};

        source.extract_path = path;

        source.search_paths.push_back(path);

        return source;
    }

    if (is_arcade_manifest(path))
        return open_arcade_manifest(iris, path);

    if (!archive::is_archive(path))
        return {};

    ArchiveIndex index;

    index.path = path;

    if (!archive::list(index.path, &index.entries)) {
        iris_error(&iris->log.emu, "Couldn't open archive \"{}\"", path.string().c_str());

        return {};
    }

    source.id = identify_arcade_archive(index);

    if (!source.id.size())
        return {};

    load_arcade_definition(&source);

    source.archives.push_back(std::move(index));

    source.extract_path = std::filesystem::path(iris->paths.pref_path) / "arcade" / source.id;

    source.search_paths.push_back(source.extract_path);
    source.search_paths.push_back(path.parent_path());
    source.search_paths.push_back(path.parent_path() / source.id);

    if (lowercase(path.stem().string()) != source.id)
        source.search_paths.push_back(path.parent_path() / path.stem());

    index_sibling_archives(iris, &source);

    return source;
}

static bool extract_arcade_entry(Instance* iris, const ArcadeSource& source, const ArchiveIndex& index, const archive::Entry& entry) {
    std::filesystem::path dst = source.extract_path / entry.name;

    std::error_code ec;

    uintmax_t size = std::filesystem::file_size(dst, ec);

    if (!ec && size == entry.size)
        return true;

    iris_info(&iris->log.emu, "Extracting \"{}\" from \"{}\"...",
        entry.name.c_str(), index.path.filename().string().c_str());

    if (archive::extract(index.path, entry, dst))
        return true;

    iris_error(&iris->log.emu, "Couldn't extract \"{}\" from \"{}\"",
        entry.name.c_str(), index.path.string().c_str());

    return false;
}

static bool extract_arcade_source(Instance* iris, const ArcadeSource& source) {
    if (source.archives.empty())
        return true;

    std::error_code ec;

    std::filesystem::create_directories(source.extract_path, ec);

    for (const archive::Entry& entry : source.archives.front().entries) {
        if (entry.directory)
            continue;

        if (!extract_arcade_entry(iris, source, source.archives.front(), entry))
            return false;
    }

    const ArcadeFileNames& names = source.names;

    for (const std::vector <std::string>& role : { names.bios, names.nand, names.dongle, names.media, names.loader }) {
        if (!find_extracted_file(source, role).empty())
            continue;

        for (size_t i = 1; i < source.archives.size(); i++) {
            const archive::Entry* entry = nullptr;

            for (const std::string& name : role) {
                entry = find_archive_entry(source.archives[i], name);

                if (entry)
                    break;
            }

            if (!entry)
                continue;

            if (!extract_arcade_entry(iris, source, source.archives[i], *entry))
                return false;

            break;
        }
    }

    return true;
}

static std::filesystem::path prepare_dongle(Instance* iris, std::filesystem::path dump) {
    const uint32_t page_size = fs::mkfs::PAGE_SIZE + fs::mkfs::PAGE_ECC_SIZE;

    std::error_code ec;
    uintmax_t size = std::filesystem::file_size(dump, ec);

    if (ec)
        return dump;

    if (size % page_size == 0)
        return dump;

    if (size % fs::mkfs::PAGE_SIZE) {
        iris_error(&iris->log.emu, "Dongle \"{}\" isn't a whole number of pages", dump.string().c_str());

        return dump;
    }

    std::filesystem::path card = dump.parent_path() / "dongle.ps2";

    if (std::filesystem::exists(card) &&
        std::filesystem::last_write_time(card, ec) >= std::filesystem::last_write_time(dump, ec))
        return card;

    std::filesystem::path ecc_path = dump.parent_path() /
        (dump.stem().string() + "_spr" + dump.extension().string());

    uintmax_t pages = size / fs::mkfs::PAGE_SIZE;

    FILE* data_file = fopen(dump.string().c_str(), "rb");

    if (!data_file)
        return dump;

    FILE* ecc_file = fopen(ecc_path.string().c_str(), "rb");

    if (!ecc_file) {
        iris_info(&iris->log.emu, "No ECC dump for \"{}\", generating one", dump.filename().string().c_str());
    }

    FILE* out = fopen(card.string().c_str(), "wb");

    if (!out) {
        fclose(data_file);

        if (ecc_file)
            fclose(ecc_file);

        return dump;
    }

    std::vector<uint8_t> page(page_size);

    for (uintmax_t i = 0; i < pages; i++) {
        if (fread(page.data(), 1, fs::mkfs::PAGE_SIZE, data_file) != fs::mkfs::PAGE_SIZE)
            break;

        uint8_t* ecc = page.data() + fs::mkfs::PAGE_SIZE;

        if (!ecc_file || fread(ecc, 1, fs::mkfs::PAGE_ECC_SIZE, ecc_file) != fs::mkfs::PAGE_ECC_SIZE)
            fs::mkfs::page_ecc(page.data(), ecc);

        fwrite(page.data(), 1, page_size, out);
    }

    fclose(out);
    fclose(data_file);

    if (ecc_file)
        fclose(ecc_file);

    iris_info(&iris->log.emu, "Converted dongle dump into \"{}\"", card.string().c_str());

    return card;
}

static bool unpack_dongle(Instance* iris, const std::filesystem::path& path,
    const std::filesystem::path& out, const std::string& bootprog) {
    fs::blk::Device* dev = fs::blk::open_file(iris->logger, path.string().c_str());

    if (!dev) {
        iris_warning(&iris->log.emu, "Couldn't open dongle \"{}\" to look for \"{}\"",
            path.string().c_str(), bootprog.c_str());

        return false;
    }

    fs::Fs* filesystem = fs::probe(iris->logger, dev, true);

    if (!filesystem) {
        iris_warning(&iris->log.emu, "Dongle \"{}\" doesn't hold a readable memory card filesystem",
            path.string().c_str());

        fs::blk::close(dev);

        return false;
    }

    std::vector <fs::Entry> entries;

    int r = fs::list(filesystem, "/", &entries);

    if (r != fs::FS_OK) {
        iris_warning(&iris->log.emu, "Couldn't read the dongle's root directory ({})", fs::error_name(r));

        fs::close(filesystem);

        return false;
    }

    std::error_code ec;

    std::filesystem::create_directories(out, ec);

    bool found = false;

    std::vector <uint8_t> buf;

    for (const fs::Entry& entry : entries) {
        if (entry.flags & fs::ENTRY_DIRECTORY)
            continue;

        std::string name;

        if (!fs::sanitize_name(entry.name, &name))
            continue;

        iris_debug(&iris->log.emu, "Dongle holds \"{}\" ({} bytes)", name.c_str(), entry.size);

        fs::Handle* handle = nullptr;

        if (fs::open(filesystem, ("/" + name).c_str(), &handle) != fs::FS_OK)
            continue;

        buf.resize((size_t)entry.size);

        int64_t got = entry.size ? fs::read(filesystem, handle, 0, buf.data(), entry.size) : 0;

        fs::close_handle(filesystem, handle);

        if (got < 0) {
            iris_warning(&iris->log.emu, "Couldn't read \"{}\" off the dongle", name.c_str());

            continue;
        }

        FILE* file = fopen((out / name).string().c_str(), "wb");

        if (!file) {
            iris_warning(&iris->log.emu, "Couldn't write \"{}\" out of the dongle", name.c_str());

            continue;
        }

        if (got)
            fwrite(buf.data(), 1, (size_t)got, file);

        fclose(file);

        if (bootprog == name)
            found = true;
    }

    fs::close(filesystem);

    return found;
}


static int boot_arcade_thread(Instance* iris, std::string path) {
    if (!load_arcade_files(iris, path))
        finish_load(iris, 1);

    return 0;
}

static bool load_arcade_source(Instance* iris, const ArcadeSource& source) {
    const std::string& name = source.name;

    iris->arcade_id = "";

    cdvd::close(iris->ps2->cdvd);

    clean_tmp_files(iris);

    std::string set = arcade::resolve_set_name(source.id);

    std::string gameid = query_arcade_value<std::string>(source.id, "gameid").value_or("");

    if (gameid.size()) {
        iris_info(&iris->log.emu, "Loading arcade game \"{}\" ({}, {})...", name.c_str(), source.id.c_str(), gameid.c_str());
    } else {
        iris_info(&iris->log.emu, "Loading arcade game \"{}\" ({})...", name.c_str(), source.id.c_str());
    }

    if (!extract_arcade_source(iris, source)) {
        push_info(iris, "Couldn't start arcade game (Extraction failed)");

        return false;
    }

    int system = source.system;

    if (!iris->enable_magicgate && (system == ps2::KONAMI_PYTHON || system == ps2::KONAMI_PYTHON2)) {
        iris_warning(&iris->log.emu, "MagicGate is disabled, {} games need it to boot",
            get_system_name(iris, system));

        push_info(iris, "MagicGate is disabled, this arcade game may not work");
    }

    ArcadeFiles files = resolve_arcade_files(iris, source);

    std::error_code ec;

    std::filesystem::create_directories(files.sram.parent_path(), ec);
    std::filesystem::create_directories(files.nvram.parent_path(), ec);
    std::filesystem::create_directories(files.bbsram.parent_path(), ec);

    switch (system) {
        case ps2::NAMCO_SYSTEM_147:
        case ps2::NAMCO_SYSTEM_148: {
            if (!std::filesystem::exists(files.bios)) {
                iris_error(&iris->log.emu, "Couldn't find bootrom file \"{}\"", files.bios.string().c_str());

                push_info(iris, "Couldn't start arcade game (Missing bootrom)");

                return false;
            }

            if (!std::filesystem::exists(files.nand)) {
                iris_error(&iris->log.emu, "Couldn't find NAND file \"{}\"", files.nand.string().c_str());

                push_info(iris, "Couldn't start arcade game (Missing NAND)");

                return false;
            }

            ps2::load_bios(iris->ps2, files.bios.string().c_str());
            ps2::set_system(iris->ps2, system);
            s14x::nand::load(iris->ps2->s14x_nand, files.nand.string().c_str());
            s14x::sram::load(iris->ps2->s14x_sram, files.sram.string().c_str());

            cdvd::load_nvram(iris->ps2->cdvd, files.nvram.string().c_str());

            // Read the console and i.Link IDs out of the NVM
            settings::apply_mg_keys(iris);

            if (iris->ps2->s14x_ioboard) {
                iris->ps2->s14x_ioboard->mode = source.ioboard_mode;
            }

            ps2::reset(iris->ps2);

            iris->arcade_id = set.size() ? set : source.id;

            finish_load(iris, 0, name + " (" + source.id + ")");

            return true;
        } break;

        case ps2::KONAMI_PYTHON2: {
            ps2::load_bios(iris->ps2, files.bios.string().c_str());
            ps2::set_system(iris->ps2, system);

            cdvd::load_nvram(iris->ps2->cdvd, files.nvram.string().c_str());

            // Read the console and i.Link IDs out of the NVM
            settings::apply_mg_keys(iris);

            if (!speed::load_hdd(iris->ps2->speed, files.media.string().c_str())) {
                iris_error(&iris->log.emu, "Couldn't read HDD image \"{}\"", files.media.string().c_str());

                push_info(iris, "Couldn't start arcade game (Bad HDD image)");

                return false;
            }

            if (!speed::load_hdd_id(iris->ps2->speed, files.hdd_id.string().c_str())) {
                push_info(iris, "Couldn't start arcade game (Bad HDD ID)");

                return false;
            }

            ps2::reset(iris->ps2);

            if (usb::get_port_device(iris->ps2->usb, 0) != usb::USB_DEVICE_P2IO) {
                iris_error(&iris->log.emu, "The Python 2 I/O board isn't attached");

                push_info(iris, "Couldn't start arcade game (No I/O board)");

                return false;
            }

            usb::p2io_set_input_type(iris->ps2->usb, source.input_type);
            usb::p2io_set_dongle(iris->ps2->usb, kp2::p2io::DONGLE_BLACK, files.dongle_black.string().c_str());

            if (source.needs_white_dongle)
                usb::p2io_set_dongle(iris->ps2->usb, kp2::p2io::DONGLE_WHITE, files.dongle_white.string().c_str());

            kp2::p2io::P2io* p2io = kp2::p2io::from_device(&iris->ps2->usb->device[0]);

            kp2::p2io::set_dip_switches(p2io, (uint8_t)source.dip_switches);
            kp2::p2io::set_force_31khz(p2io, source.force_31khz);

            if (!p2io->dongle_loaded[kp2::p2io::DONGLE_BLACK]) {
                push_info(iris, "Couldn't start arcade game (Bad black dongle)");

                return false;
            }

            if (ata::disc::is_compressed(files.media.string().c_str()))
                push_info(iris, "Compressed HDD images are read only, progress won't be saved");

            iris->arcade_id = set.size() ? set : source.id;

            finish_load(iris, 0, name + " (" + source.id + ")");

            return true;
        } break;

        case ps2::KONAMI_PYTHON: {
            if (!std::filesystem::exists(files.bios)) {
                iris_error(&iris->log.emu, "Couldn't find bootrom file \"{}\"", files.bios.string().c_str());

                push_info(iris, "Couldn't start arcade game (Missing bootrom)");

                return false;
            }

            ps2::load_bios(iris->ps2, files.bios.string().c_str());
            ps2::set_system(iris->ps2, system);

            if (!std::filesystem::exists(files.nvram)) {
                std::filesystem::path bios_stem = files.bios.parent_path() / files.bios.stem();
                std::filesystem::path sibling = get_rom_path(bios_stem, "nvm");

                if (!sibling.empty()) {
                    std::filesystem::copy_file(sibling, files.nvram, ec);

                    iris_info(&iris->log.emu, "Seeded NVRAM from \"{}\"", sibling.string().c_str());
                }
            }

            cdvd::load_nvram(iris->ps2->cdvd, files.nvram.string().c_str());

            if (fw::get_port_device(iris->ps2->fw, 0) != fw::DEVICE_P1IO) {
                iris_error(&iris->log.emu, "The Python 1 I/O board isn't attached");

                push_info(iris, "Couldn't start arcade game (No I/O board)");

                return false;
            }

            kp1::p1io::P1io* p1io = kp1::p1io::from_device(&iris->ps2->fw->device[0]);

            kp1::p1io::set_io_mode(p1io, source.io_mode);

            if (std::filesystem::exists(files.io_configrom)) {
                kp1::p1io::load_config_rom(p1io, files.io_configrom.string().c_str());
            } else {
                iris_info(&iris->log.emu, "No i.LINK config ROM for \"{}\", using the built-in one", source.id.c_str());
            }

            if (std::filesystem::exists(files.io_bootrom)) {
                kp1::p1io::load_bootrom(p1io, files.io_bootrom.string().c_str());
            } else {
                iris_warning(&iris->log.emu, "No I/O board boot ROM for \"{}\"", source.id.c_str());
            }

            if (std::filesystem::exists(files.dongle_int)) {
                kp1::p1io::load_dongle(p1io, kp1::p1io::DONGLE_INTERNAL, files.dongle_int.string().c_str());
            }

            if (std::filesystem::exists(files.dongle_ext)) {
                kp1::p1io::load_dongle(p1io, kp1::p1io::DONGLE_EXTERNAL, files.dongle_ext.string().c_str());
            }

            if (!std::filesystem::exists(files.bbsram) && std::filesystem::exists(files.bbsram_seed)) {
                std::filesystem::copy_file(files.bbsram_seed, files.bbsram, ec);
            }

            if (!std::filesystem::exists(files.bbsram)) {
                iris_warning(&iris->log.emu, "No timekeeper dump for \"{}\", most games won't boot without one", source.id.c_str());
            }

            kp1::p1io::load_bbsram(p1io, files.bbsram.string().c_str());

            int media_loaded = source.media_hdd
                ? kp1::p1io::load_hdd(p1io, files.media.string().c_str())
                : kp1::p1io::load_cf(p1io, files.media.string().c_str());

            if (!media_loaded) {
                iris_error(&iris->log.emu, "Couldn't read media image \"{}\"", files.media.string().c_str());

                push_info(iris, "Couldn't start arcade game (Bad media image)");

                return false;
            }

            std::filesystem::path dongle = prepare_dongle(iris, files.dongle);

            std::filesystem::path dongle_files =
                std::filesystem::path(iris->paths.pref_path) / "arcade" / "dongle" / source.id;

            bool dongle_boot = arcade_boots_from_dongle(iris, source);

            if (dongle_boot && !unpack_dongle(iris, dongle, dongle_files, source.bootprog)) {
                iris_error(&iris->log.emu, "Dongle \"{}\" doesn't hold boot program \"{}\"",
                    dongle.string().c_str(), source.bootprog.c_str());

                push_info(iris, "Couldn't start arcade game (Boot program not on dongle)");

                return false;
            }

            std::filesystem::path card = std::filesystem::path(iris->paths.pref_path) / "arcade" / "card" / (source.id + ".ps2");

            std::filesystem::create_directories(card.parent_path(), ec);

            if (!std::filesystem::exists(card)) {
                std::filesystem::copy_file(dongle, card, ec);

                iris_info(&iris->log.emu, "Copied dongle to \"{}\"", card.string().c_str());
            }

            attach_memory_card(iris, 0, card.string().c_str());

            iris->paths.mecha_card_id_path = files.mcd_id.string();

            settings::apply_mg_keys(iris);

            if (dongle_boot) {
                std::filesystem::path program = dongle_files / source.bootprog;

                ps2::iop_map_device(iris->ps2, "mc0", dongle_files.string().c_str());
                ps2::iop_map_device(iris->ps2, "host", dongle_files.string().c_str());
                ps2::iop_map_device(iris->ps2, "host0", dongle_files.string().c_str());

                elf::load_symbols_from_file(iris, program.string());

                iris_info(&iris->log.emu, "Booting \"{}\" off the dongle", source.bootprog.c_str());

                ps2::boot_file(iris->ps2, ("host:  " + program.string()).c_str());

                std::string dongle_arg = "mc0:" + source.bootprog;

                const char* boot_args[] = { dongle_arg.c_str(), "DANGLE" };

                ps2::set_boot_args(iris->ps2, boot_args, 2);
            } else {
                ps2::reset(iris->ps2);
            }

            iris->arcade_id = set.size() ? set : source.id;

            finish_load(iris, 0, name + " (" + source.id + ")");

            return true;
        } break;

        case ps2::NAMCO_SYSTEM_246:
        case ps2::NAMCO_SYSTEM_256:
        case ps2::NAMCO_SYSTEM_SUPER_256: {
            ps2::load_bios(iris->ps2, files.bios.string().c_str());
            ps2::set_system(iris->ps2, system);

            cdvd::load_nvram(iris->ps2->cdvd, files.nvram.string().c_str());

            std::filesystem::path dongle = prepare_dongle(iris, files.dongle);

            bool dongle_boot = arcade_boots_from_dongle(iris, source);

            std::filesystem::path dongle_files =
                std::filesystem::path(iris->paths.pref_path) / "arcade" / "dongle" / source.id;

            if (dongle_boot && !unpack_dongle(iris, dongle, dongle_files, source.bootprog)) {
                iris_error(&iris->log.emu, "Dongle \"{}\" doesn't hold boot program \"{}\"",
                    dongle.string().c_str(), source.bootprog.c_str());

                push_info(iris, "Couldn't start arcade game (Boot program not on dongle)");

                return false;
            }

            attach_memory_card(iris, 0, dongle.string().c_str());

            // Read the console and i.Link IDs out of the NVM
            settings::apply_mg_keys(iris);

            if (!s2x6::acata::load(iris->ps2->s2x6_acata, files.media.string().c_str(), files.media_type)) {
                iris_error(&iris->log.emu, "Couldn't read media image \"{}\"", files.media.string().c_str());

                return false;
            }

            s2x6::acsram::load(iris->ps2->s2x6_acsram, files.sram.string().c_str());

            s2x6::acuart::set_device(iris->ps2->s2x6_acuart, source.uart_device);

            s2x6::acjv::set_mode(iris->ps2->s2x6_acjv, source.jvs_mode, source.wheel_style);

            settings::apply_arcade_dip_switches(iris);

            s2x6::acjv::set_gun_buttons(iris->ps2->s2x6_acjv, source.gun_trigger, source.gun_pedal);
            s2x6::acjv::set_gun_board(iris->ps2->s2x6_acjv, source.gun_board, source.gun_sensor, source.gun_sensor_active_high);

            if (dongle_boot) {
                std::filesystem::path program = dongle_files / source.bootprog;

                ps2::iop_map_device(iris->ps2, "mc0", dongle_files.string().c_str());
                ps2::iop_map_device(iris->ps2, "host", dongle_files.string().c_str());
                ps2::iop_map_device(iris->ps2, "host0", dongle_files.string().c_str());

                elf::load_symbols_from_file(iris, program.string());

                iris_info(&iris->log.emu, "Booting \"{}\" off the dongle", source.bootprog.c_str());

                ps2::boot_file(iris->ps2, ("host:  " + program.string()).c_str());

                std::string dongle_arg = "mc0:" + source.bootprog;

                const char* boot_args[] = { dongle_arg.c_str(), "DANGLE" };

                ps2::set_boot_args(iris->ps2, boot_args, 2);
            } else {
                ps2::iop_map_device(iris->ps2, "host", files.loader.parent_path().string().c_str());

                elf::load_symbols_from_file(iris, files.loader.string());

                ps2::boot_file(iris->ps2, ("host:  " + files.loader.string()).c_str());
            }

            iris->arcade_id = set.size() ? set : source.id;

            finish_load(iris, 0, name + " (" + source.id + ")");

            return true;
        } break;

        default: {
            iris_error(&iris->log.emu, "{} isn't supported yet", get_system_name(iris, system));
        } break;
    }

    return false;
}

bool is_arcade_file(Instance* iris, std::string path) {
    if (is_arcade_manifest(path))
        return true;

    std::error_code ec;

    if (std::filesystem::is_directory(path, ec))
        return identify_arcade_directory(path).size() != 0;

    if (!archive::is_archive(path))
        return false;

    ArchiveIndex index;

    index.path = path;

    if (!archive::list(index.path, &index.entries))
        return false;

    return identify_arcade_archive(index).size() != 0;
}

static bool arcade_system_supported(int system) {
    switch (system) {
        case ps2::NAMCO_SYSTEM_147:
        case ps2::NAMCO_SYSTEM_148:
        case ps2::NAMCO_SYSTEM_246:
        case ps2::NAMCO_SYSTEM_256:
        case ps2::NAMCO_SYSTEM_SUPER_256:
        case ps2::KONAMI_PYTHON:
        case ps2::KONAMI_PYTHON2:
            return true;
    }

    return false;
}

bool load_arcade(Instance* iris, std::string path) {
    std::optional <ArcadeSource> source = open_arcade_source(iris, path);

    if (!source)
        return false;

    int system = source->system;

    if (arcade_system_supported(system)) {
        const char* missing = find_missing_arcade_file(iris, *source);

        if (missing) {
            iris_error(&iris->log.emu, "Couldn't find {} file for \"{}\"", missing, source->id.c_str());

            push_info(iris, std::string("Couldn't start arcade game (Missing ") + missing + ")");

            return false;
        }

        iris->ui.loading_target = source->id;
        iris->ui.loading_file_active = true;
        iris->load_ready = false;
        iris->debug.pause = true;

        gs::renderer::hotswap(iris->renderer, gs::renderer::BACKEND_NULL);

        imgui::start_dim(iris, 0.35f, 100);

        iris->load_pending_file = path;
        iris->load_pending_arcade = true;
        iris->load_start_pending = true;

        return true;
    }

    return load_arcade_source(iris, *source);
}

bool load_arcade_files(Instance* iris, std::string path) {
    std::optional<ArcadeSource> source = open_arcade_source(iris, path);

    if (!source)
        return false;

    return load_arcade_source(iris, *source);
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

    iris->input.mcd[slot] = dev::mcd::attach(iris->logger, iris->ps2->sio2, slot+2, path);

    iris->input.mcd_slot_type[slot] = 1;

    settings::apply_card_magicgate(iris, slot);

    return 1;
}

void detach_memory_card(Instance* iris, int slot) {
    iris->input.mcd_slot_type[slot] = 0;
    iris->input.mcd[slot] = nullptr;

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
    "Namco System 256",
    "Namco System Super 256",
    "Wega HVX"
};

const char* get_system_name(Instance* iris, int system) {
    if (system < 0 || system >= (int)(sizeof(g_system_names) / sizeof(*g_system_names)))
        return "Unknown";

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
        case ps2::NAMCO_SYSTEM_SUPER_256:
        case ps2::WEGA_HVX:
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