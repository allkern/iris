#include <toml++/toml.hpp>

#include "ps2.h"

const toml::table g_arcade_definitions = toml::table {
    { "pacmanap", toml::table {
        { "system", PS2_SYSTEM_NAMCO_S147 },
        { "name", "Pac-Man's Arcade Party" },
        { "nand", "kp007a_k9k8g08u0b_pmaam12-na-c.ic26" },
        { "bios", "common_system147b_bootrom.ic1" },
        { "boot", "atfile0:PMAAC.elf" }
    }},
    { "pacmanbr", toml::table {
        { "system", PS2_SYSTEM_NAMCO_S147 },
        { "name", "Pac-Man: Battle Royale" },
        { "nand", "pbr102-2-na-mpro-a13_kp006b.ic26" },
        { "bios", "common_system147b_bootrom.ic1" },
        { "boot", "atfile0:pacmanBR.elf" }
    }},
    { "akaiser", toml::table {
        { "system", PS2_SYSTEM_NAMCO_S147 },
        { "name", "Animal Kaiser" },
        { "nand", "kp005a_ana1004-na-b.ic26" },
        { "bios", "common_system147b_bootrom.ic1" },
        { "boot", "atfile0:main.elf" }
    }},
    { "akaievo", toml::table {
        { "system", PS2_SYSTEM_NAMCO_S147 },
        { "name", "Animal Kaiser Evolution" },
        { "nand", "kp012b_k9k8g08u0b.ic31" },
        { "bios", "common_system147b_bootrom.ic1" },
        { "boot", "atfile0:main.elf" }
    }},
    { "umilucky", toml::table {
        { "system", PS2_SYSTEM_NAMCO_S148 },
        { "name", "Umimonogatari Lucky Marine Theater" },
        { "nand", "uls100-1-na-mpro-b01_kp008a.ic31" },
        { "bios", "common_system148_bootrom.ic1" },
        { "boot", "atfile0:prog.elf" }
    }}
};