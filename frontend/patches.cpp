#include <cctype>
#include <cstdint>

#include "iris.hpp"
#include "patches.hpp"

#include "ps2.hpp"
#include "ee/bus.hpp"
#include "iop/disc.hpp"

namespace iris::patches {

struct Patch {
    const char* serial;
    uint32_t addr;
    uint32_t value;
};

static const Patch patch_db[] = {
    // Dead or Alive 2 (Japan/Europe/USA)
    // These games require cache emulation in order to work.
    // Here's a great post explaining how the games break and why the patches work:
    // https://www.freestepdodge.com/threads/sins-of-the-ps2-dead-or-alive-2-ps2-emulation-issue-discussion.8700/
    { "SLPS-25002", 0x00290408, 0x24060000 },
    { "SCES-50003", 0x002b4c44, 0x24060000 },
    { "SLUS-20071", 0x002b06ec, 0x24060000 },
};

std::string get_serial(const char* boot_path) {
    std::string name = boot_path;

    size_t start = name.find_last_of("\\/:");

    if (start != std::string::npos) {
        name = name.substr(start + 1);
    }

    size_t end = name.find(';');

    if (end != std::string::npos) {
        name.resize(end);
    }

    std::string serial;

    for (char c : name) {
        if (c == '_') {
            serial += '-';
        } else if (c != '.' && !isspace((unsigned char)c)) {
            serial += (char)toupper((unsigned char)c);
        }
    }

    return serial;
}

void clear(Instance* iris) {
    ee::bus::clear_patches(iris->ps2->ee_bus);
}

void apply(Instance* iris, const char* boot_path) {
    clear(iris);

    if (!iris->enable_patches || !boot_path) {
        return;
    }

    std::string serial = get_serial(boot_path);

    int count = 0;

    for (const Patch& patch : patch_db) {
        if (serial != patch.serial) {
            continue;
        }

        ee::bus::set_patch(iris->ps2->ee_bus, patch.addr, patch.value);

        count++;
    }

    if (count) {
        iris_info(&iris->log.emu, "Applied {} patch(es) for {}", count, serial.c_str());

        std::string msg = "Applied " + std::to_string(count) + (count == 1 ? " patch for " : " patches for ") + serial;

        push_info(iris, msg);
    }
}

void reapply(Instance* iris) {
    if (!iris->ps2->cdvd->disc) {
        clear(iris);

        return;
    }

    apply(iris, iop::disc::get_boot_path(iris->ps2->cdvd->disc));
}

void insert(Instance* iris) {
    if (!iris->apply_patches_on_insert) {
        return;
    }

    reapply(iris);
}

}
