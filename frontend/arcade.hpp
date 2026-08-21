#include <toml++/toml.hpp>

#include "ps2.hpp"

const toml::table g_arcade_definitions = toml::table {
    // Namco System 147/148
    { "pacmanap", toml::table {
        { "system", iris::ps2::NAMCO_SYSTEM_147 },
        { "name", "Pac-Man's Arcade Party" },
        { "nand", "kp007a_k9k8g08u0b_pmaam12-na-c.ic26" },
        { "bios", "common_system147b_bootrom.ic1" },
        { "boot", "atfile0:PMAAC.elf" }
    }},
    { "pacmanbr", toml::table {
        { "system", iris::ps2::NAMCO_SYSTEM_147 },
        { "name", "Pac-Man: Battle Royale" },
        { "nand", "pbr102-2-na-mpro-a13_kp006b.ic26" },
        { "bios", "common_system147b_bootrom.ic1" },
        { "boot", "atfile0:pacmanBR.elf" }
    }},
    { "akaiser", toml::table {
        { "system", iris::ps2::NAMCO_SYSTEM_147 },
        { "name", "Animal Kaiser: The King of Animals" },
        { "nand", "kp005a_ana1004-na-b.ic26" },
        { "bios", "common_system147b_bootrom.ic1" },
        { "boot", "atfile0:main.elf" },
        { "ioboard_mode", 1 }
    }},
    { "akaievo", toml::table {
        { "system", iris::ps2::NAMCO_SYSTEM_147 },
        { "name", "Animal Kaiser Evolution" },
        { "nand", "kp012b_k9k8g08u0b.ic31" },
        { "bios", "common_system147b_bootrom.ic1" },
        { "boot", "atfile0:main.elf" },
        { "ioboard_mode", 1 }
    }},
    { "umilucky", toml::table {
        { "system", iris::ps2::NAMCO_SYSTEM_148 },
        { "name", "Umimonogatari Lucky Marine Theater" },
        { "nand", "uls100-1-na-mpro-b01_kp008a.ic31" },
        { "bios", "common_system148_bootrom.ic1" },
        { "boot", "atfile0:prog.elf" }
    }},

    // Namco System 246/256/Super 256
    { "rrvac", toml::table {
        { "system", iris::ps2::NAMCO_SYSTEM_246 },
        { "name", "Ridge Racer V Arcade Battle (RRV3 Ver. A)" },
        { "gameid", "NM00001" },
        { "bootprog", "START" },
        { "uart_device", iris::s2x6::acuart::DEVICE_DRIVE_BOARD },
        { "jvs_mode", iris::s2x6::acjv::MODE_FCA },
        { "bios", "r27v1602f.7d" },
        { "dongle", "rrv3vera.ic002" },
        { "media", "rrv1-a.chd" },
        { "media_type", iris::s2x6::acata::MEDIA_CD }
    }},
    { "rrvac2", toml::table {
        { "system", iris::ps2::NAMCO_SYSTEM_246 },
        { "name", "Ridge Racer V Arcade Battle (RRV2 Ver. A)" },
        { "gameid", "NM00001" },
        { "bootprog", "START" },
        { "uart_device", iris::s2x6::acuart::DEVICE_DRIVE_BOARD },
        { "jvs_mode", iris::s2x6::acjv::MODE_FCA },
        { "bios", "r27v1602f.7d" },
        { "dongle", "rrv2vera.ic002" },
        { "media", "rrv1-a.chd" },
        { "media_type", iris::s2x6::acata::MEDIA_CD }
    }},
    { "rrvac1", toml::table {
        { "system", iris::ps2::NAMCO_SYSTEM_246 },
        { "name", "Ridge Racer V Arcade Battle (RRV1 Ver. A)" },
        { "gameid", "NM00001" },
        { "bootprog", "START" },
        { "uart_device", iris::s2x6::acuart::DEVICE_DRIVE_BOARD },
        { "jvs_mode", iris::s2x6::acjv::MODE_FCA },
        { "bios", "r27v1602f.7d" },
        { "dongle", "rrv1vera.ic002" },
        { "media", "rrv1-a.chd" },
        { "media_type", iris::s2x6::acata::MEDIA_CD }
    }},
    { "vnight", toml::table {
        { "system", iris::ps2::NAMCO_SYSTEM_246 },
        { "name", "Vampire Night (VPN3 Ver. B)" },
        { "gameid", "NM00003" },
        { "bootprog", "VPNGAME" },
        { "bios", "r27v1602f.7d" },
        { "dongle", "vpn3verb.ic002" },
        { "media", "vpn1cd0.chd" },
        { "media_type", iris::s2x6::acata::MEDIA_CD },
        { "jvs_mode", iris::s2x6::acjv::MODE_LIGHTGUN },
        { "gun_trigger", iris::s2x6::acjv::BTN_2 },
        { "gun_pedal", 0 },
        { "gun_board", iris::s2x6::acjv::GUN_BOARD_CAMERA },
        { "gun_sensor", 0x0200 },
        { "gun_sensor_active_high", 1 }
    }},
    { "bldyr3b", toml::table {
        { "system", iris::ps2::NAMCO_SYSTEM_246 },
        { "name", "Bloody Roar 3 (bootleg)" },
        { "gameid", "NM00002" },
        { "bootprog", "BDRGAME" },
        { "bios", "r27v1602f.7d" },
        { "dongle", "br3-dongle.bin" },
        { "media", "bldyr3b.chd" },
        { "media_type", iris::s2x6::acata::MEDIA_CD }
    }},
    { "tekken4", toml::table {
        { "system", iris::ps2::NAMCO_SYSTEM_246 },
        { "name", "Tekken 4 (TEF3 Ver. C)" },
        { "gameid", "NM00004" },
        { "bootprog", "TK4LOAD" },
        { "bios", "r27v1602f.7d" },
        { "dongle", "tef3verc.ic002" },
        { "media", "tef1dvd0.chd" },
        { "media_type", iris::s2x6::acata::MEDIA_DVD }
    }},
    { "tekken4a", toml::table {
        { "system", iris::ps2::NAMCO_SYSTEM_246 },
        { "name", "Tekken 4 (TEF2 Ver. A)" },
        { "gameid", "NM00004" },
        { "bootprog", "TK4LOAD" },
        { "bios", "r27v1602f.7d" },
        { "dongle", "tef2vera.ic002" },
        { "media", "tef1dvd0.chd" },
        { "media_type", iris::s2x6::acata::MEDIA_DVD }
    }},
    { "tekken4b", toml::table {
        { "system", iris::ps2::NAMCO_SYSTEM_246 },
        { "name", "Tekken 4 (TEF1 Ver. A)" },
        { "gameid", "NM00004" },
        { "bootprog", "TK4LOAD" },
        { "bios", "r27v1602f.7d" },
        { "dongle", "tef1vera.bin" },
        { "media", "tef1dvd0.chd" },
        { "media_type", iris::s2x6::acata::MEDIA_DVD }
    }},
    { "tekken4c", toml::table {
        { "system", iris::ps2::NAMCO_SYSTEM_246 },
        { "name", "Tekken 4 (TEF1 Ver. C)" },
        { "gameid", "NM00004" },
        { "bootprog", "TK4LOAD" },
        { "bios", "r27v1602f.7d" },
        { "dongle", "tef1verc.ic002" },
        { "media", "tef1dvd0.chd" },
        { "media_type", iris::s2x6::acata::MEDIA_DVD }
    }},
    { "wanganmd", toml::table {
        { "system", iris::ps2::NAMCO_SYSTEM_246 },
        { "name", "Wangan Midnight (WMN1 Ver. A)" },
        { "gameid", "NM00008" },
        { "bootprog", "WGNLOAD" },
        { "jvs_mode", iris::s2x6::acjv::MODE_DRIVE },
        { "wheel_style", iris::s2x6::acjv::WHEEL_WANGAN },
        { "bios", "r27v1602f.7d" },
        { "dongle", "wmn1vera.ic002" },
        { "media", "wmn1-a.chd" },
        { "media_type", iris::s2x6::acata::MEDIA_CD }
    }},
    { "wanganmr", toml::table {
        { "system", iris::ps2::NAMCO_SYSTEM_246 },
        { "name", "Wangan Midnight R (WMR1 Ver. A)" },
        { "gameid", "NM00005" },
        { "bootprog", "AL3LOAD" },
        { "jvs_mode", iris::s2x6::acjv::MODE_DRIVE },
        { "wheel_style", iris::s2x6::acjv::WHEEL_WANGAN },
        { "bios", "r27v1602f.7d" },
        { "dongle", "wmr1vera.ic002" },
        { "media", "wmr1-a.chd" },
        { "media_type", iris::s2x6::acata::MEDIA_CD }
    }},
    { "batlgr3", toml::table {
        { "system", iris::ps2::NAMCO_SYSTEM_246 },
        { "name", "Battle Gear 3 (Ver.2.01A)" },
        { "gameid", "NM00010" },
        { "bootprog", "BGRLOAD" },
        { "jvs_mode", iris::s2x6::acjv::MODE_DRIVE },
        { "bios", "r27v1602f.7d" },
        { "dongle", "batlgr3.ic002" },
        { "media", "batlgr3.chd" },
        { "media_type", iris::s2x6::acata::MEDIA_HDD }
    }},
    { "dragchrn", toml::table {
        { "system", iris::ps2::NAMCO_SYSTEM_246 },
        { "name", "Dragon Chronicles (DC001 Ver. A)" },
        { "gameid", "NM00014" },
        { "bootprog", "DGNLOAD" },
        { "bios", "r27v1602f.7d" },
        { "dongle", "dc001vera.ic002" },
        { "media", "dragchrn.chd" },
        { "media_type", iris::s2x6::acata::MEDIA_CD }
    }},
    { "netchu02c", toml::table {
        { "system", iris::ps2::NAMCO_SYSTEM_246 },
        { "name", "Netchuu Pro Yakyuu 2002 (NPY1 Ver. C)" },
        { "gameid", "NM00009" },
        { "bootprog", "NPBLOAD" },
        { "bios", "r27v1602f.7d" },
        { "dongle", "npy1verc.ic002" },
        { "media", "npy1cd0c.chd" },
        { "media_type", iris::s2x6::acata::MEDIA_CD }
    }},
    { "netchu02b", toml::table {
        { "system", iris::ps2::NAMCO_SYSTEM_246 },
        { "name", "Netchuu Pro Yakyuu 2002 (NPY1 Ver. B)" },
        { "gameid", "NM00009" },
        { "bootprog", "NPBLOAD" },
        { "bios", "r27v1602f.7d" },
        { "dongle", "npy1verb.ic002" },
        { "media", "npy1cd0b.chd" },
        { "media_type", iris::s2x6::acata::MEDIA_CD }
    }},
    { "scptour", toml::table {
        { "system", iris::ps2::NAMCO_SYSTEM_246 },
        { "name", "Smash Court Pro Tournament (SCP1)" },
        { "gameid", "NM00006" },
        { "bootprog", "SCPTLOAD" },
        { "bios", "r27v1602f.7d" },
        { "dongle", "scp1vera.ic002" },
        { "media", "scp1cd0.chd" },
        { "media_type", iris::s2x6::acata::MEDIA_CD }
    }},
    { "soulclb2", toml::table {
        { "system", iris::ps2::NAMCO_SYSTEM_246 },
        { "name", "Soul Calibur II (SC23 Ver. A)" },
        { "gameid", "NM00007" },
        { "bootprog", "SCSLOAD" },
        { "bios", "r27v1602f.7d" },
        { "dongle", "sc23vera.ic002" },
        { "media", "sc21-dvd0d.chd" },
        { "media_type", iris::s2x6::acata::MEDIA_DVD }
    }},
    { "soulcl2a", toml::table {
        { "system", iris::ps2::NAMCO_SYSTEM_246 },
        { "name", "Soul Calibur II (SC22 Ver. A)" },
        { "gameid", "NM00007" },
        { "bootprog", "SCSLOAD" },
        { "bios", "r27v1602f.7d" },
        { "dongle", "sc22vera.ic002" },
        { "media", "sc21-dvd0d.chd" },
        { "media_type", iris::s2x6::acata::MEDIA_DVD }
    }},
    { "soulcl2b", toml::table {
        { "system", iris::ps2::NAMCO_SYSTEM_246 },
        { "name", "Soul Calibur II (SC21 Ver. A)" },
        { "gameid", "NM00007" },
        { "bootprog", "SCSLOAD" },
        { "bios", "r27v1602f.7d" },
        { "dongle", "sc21vera.ic002" },
        { "media", "sc21-dvd0d.chd" },
        { "media_type", iris::s2x6::acata::MEDIA_DVD }
    }},
    { "soulcl2w", toml::table {
        { "system", iris::ps2::NAMCO_SYSTEM_246 },
        { "name", "Soul Calibur II (SC23 world version)" },
        { "gameid", "NM00007" },
        { "bootprog", "SCSLOAD" },
        { "bios", "r27v1602f.7d" },
        { "dongle", "sc23vera.ic002" },
        { "media", "sc21-dvd0b.chd" },
        { "media_type", iris::s2x6::acata::MEDIA_DVD }
    }},
    { "prdgp03", toml::table {
        { "system", iris::ps2::NAMCO_SYSTEM_246 },
        { "name", "Pride GP 2003 (PR21 Ver. A)" },
        { "gameid", "NM00011" },
        { "bootprog", "FGTLOAD" },
        { "bios", "r27v1602f.7d" },
        { "dongle", "pr21vera.ic002" },
        { "media", "pr21dvd0.chd" },
        { "media_type", iris::s2x6::acata::MEDIA_DVD }
    }},
    { "timecrs3", toml::table {
        { "system", iris::ps2::NAMCO_SYSTEM_246 },
        { "name", "Time Crisis 3 (TST1)" },
        { "gameid", "NM00012" },
        { "bootprog", "TC3LOAD" },
        { "bios", "r27v1602f.7d" },
        { "dongle", "tst1vera.ic002" },
        { "media", "tst1dvd0.chd" },
        { "media_type", iris::s2x6::acata::MEDIA_DVD },
        { "jvs_mode", iris::s2x6::acjv::MODE_LIGHTGUN },
        { "gun_trigger", iris::s2x6::acjv::BTN_2 },
        { "gun_pedal", iris::s2x6::acjv::BTN_6 },
        { "gun_board", iris::s2x6::acjv::GUN_BOARD_TWO_TIER }
    }},
    { "timecrs3e", toml::table {
        { "system", iris::ps2::NAMCO_SYSTEM_246 },
        { "name", "Time Crisis 3 (TST2 Ver. A)" },
        { "gameid", "NM00012" },
        { "bootprog", "TC3LOAD" },
        { "bios", "r27v1602f.7d" },
        { "dongle", "tst2vera.ic002" },
        { "media", "tst1dvd0.chd" },
        { "media_type", iris::s2x6::acata::MEDIA_DVD },
        { "jvs_mode", iris::s2x6::acjv::MODE_LIGHTGUN },
        { "gun_trigger", iris::s2x6::acjv::BTN_2 },
        { "gun_pedal", iris::s2x6::acjv::BTN_6 },
        { "gun_board", iris::s2x6::acjv::GUN_BOARD_TWO_TIER }
    }},
    { "timecrs3u", toml::table {
        { "system", iris::ps2::NAMCO_SYSTEM_246 },
        { "name", "Time Crisis 3 (TST3 Ver. A)" },
        { "gameid", "NM00012" },
        { "bootprog", "TC3LOAD" },
        { "bios", "r27v1602f.7d" },
        { "dongle", "tst3vera.ic002" },
        { "media", "tst1dvd0.chd" },
        { "media_type", iris::s2x6::acata::MEDIA_DVD },
        { "jvs_mode", iris::s2x6::acjv::MODE_LIGHTGUN },
        { "gun_trigger", iris::s2x6::acjv::BTN_2 },
        { "gun_pedal", iris::s2x6::acjv::BTN_6 },
        { "gun_board", iris::s2x6::acjv::GUN_BOARD_TWO_TIER }
    }},
    { "zgundm", toml::table {
        { "system", iris::ps2::NAMCO_SYSTEM_246 },
        { "name", "Mobile Suit Z-Gundam: A.E.U.G. vs Titans (ZGA1 Ver. A)" },
        { "gameid", "NM00013" },
        { "bootprog", "GDMLOAD" },
        { "bios", "r27v1602f.7d" },
        { "dongle", "zga1vera.ic002" },
        { "media", "zga1dvd0.chd" },
        { "media_type", iris::s2x6::acata::MEDIA_DVD }
    }},
    { "zgundmdx", toml::table {
        { "system", iris::ps2::NAMCO_SYSTEM_246 },
        { "name", "Mobile Suit Z-Gundam: A.E.U.G. vs Titans DX (ZDX1 Ver. A)" },
        { "gameid", "NM00017" },
        { "bootprog", "GDXLOAD" },
        { "bios", "r27v1602f.7d" },
        { "dongle", "zdx1vera.ic002" },
        { "media", "zdx1dvd0.chd" },
        { "media_type", iris::s2x6::acata::MEDIA_DVD }
    }},
    { "fghtjam", toml::table {
        { "system", iris::ps2::NAMCO_SYSTEM_246 },
        { "name", "Capcom Fighting Jam (JAM1 Ver. A)" },
        { "gameid", "NM00018" },
        { "bootprog", "FJMLOAD" },
        { "bios", "r27v1602f.7d" },
        { "dongle", "jam1vera.ic002" },
        { "media", "jam1-dvd0.chd" },
        { "media_type", iris::s2x6::acata::MEDIA_DVD }
    }},
    { "sukuinuf", toml::table {
        { "system", iris::ps2::NAMCO_SYSTEM_246 },
        { "name", "Quiz and Variety Suku Suku Inufuku 2 (IN2 Ver. A)" },
        { "gameid", "NM00037" },
        { "bootprog", "DR2LOAD" },
        { "bios", "r27v1602f.7d" },
        { "dongle", "in2vera.ic002" },
        { "media", "hm-in2.chd" },
        { "media_type", iris::s2x6::acata::MEDIA_CD }
    }},
    { "zoidsinf", toml::table {
        { "system", iris::ps2::NAMCO_SYSTEM_246 },
        { "name", "Zoids Infinity" },
        { "gameid", "NM00016" },
        { "bootprog", "ZOILOAD" },
        { "bios", "r27v1602f.7d" },
        { "dongle", "b3900076a.ic002" },
        { "media", "zoidsinf.chd" },
        { "media_type", iris::s2x6::acata::MEDIA_HDD }
    }},
    { "zoidiexp", toml::table {
        { "system", iris::ps2::NAMCO_SYSTEM_246 },
        { "name", "Zoids Infinity EX Plus (ver. 2.10)" },
        { "gameid", "NM00025" },
        { "bootprog", "ZO2LOAD" },
        { "bios", "r27v1602f.7d" },
        { "dongle", "b3900107a.ic002" },
        { "media", "zoidsinf-ex-plus-ver2-10.chd" },
        { "media_type", iris::s2x6::acata::MEDIA_HDD }
    }},
    { "gundzaft", toml::table {
        { "system", iris::ps2::NAMCO_SYSTEM_246 },
        { "name", "Gundam Seed: Federation vs. Z.A.F.T. (SED1 Ver. A)" },
        { "gameid", "NM00024" },
        { "bootprog", "SEDLOAD" },
        { "bios", "r27v1602f.7d" },
        { "dongle", "sed1vera.ic002" },
        { "media", "sed1dvd0.chd" },
        { "media_type", iris::s2x6::acata::MEDIA_DVD }
    }},
    { "soulclb3", toml::table {
        { "system", iris::ps2::NAMCO_SYSTEM_246 },
        { "name", "Soul Calibur III: Arcade Edition (SC31001-NA-A key, NA-B disc)" },
        { "gameid", "NM00031" },
        { "bootprog", "SC3LOAD" },
        { "bios", "r27v1602f.7d" },
        { "dongle", "sc31001-na-a.ic002" },
        { "media", "sc31001-na-dvd0-b.chd" },
        { "media_type", iris::s2x6::acata::MEDIA_DVD }
    }},
    { "soulclb3a", toml::table {
        { "system", iris::ps2::NAMCO_SYSTEM_246 },
        { "name", "Soul Calibur III: Arcade Edition (SC31002-NA-A key, NA-B disc)" },
        { "gameid", "NM00031" },
        { "bootprog", "SC3LOAD" },
        { "bios", "r27v1602f.7d" },
        { "dongle", "sc31002-na-a.ic002" },
        { "media", "sc31001-na-dvd0-b.chd" },
        { "media_type", iris::s2x6::acata::MEDIA_DVD }
    }},
    { "soulclb3b", toml::table {
        { "system", iris::ps2::NAMCO_SYSTEM_246 },
        { "name", "Soul Calibur III: Arcade Edition (SC31002-NA-A key, NA-A disc)" },
        { "gameid", "NM00031" },
        { "bootprog", "SC3LOAD" },
        { "bios", "r27v1602f.7d" },
        { "dongle", "sc31001-na-a.ic002" },
        { "media", "sc31001-na-dvd0-a.chd" },
        { "media_type", iris::s2x6::acata::MEDIA_DVD }
    }},
    { "qgundam", toml::table {
        { "system", iris::ps2::NAMCO_SYSTEM_246 },
        { "name", "Quiz Mobile Suit Gundam: Monsenshi (QG1 Ver. A)" },
        { "gameid", "NM00030" },
        { "bootprog", "GQZLOAD" },
        { "bios", "r27v1602f.7d" },
        { "dongle", "qg1vera.ic002" },
        { "media", "qg1.chd" },
        { "media_type", iris::s2x6::acata::MEDIA_DVD }
    }},
    { "kinniku2", toml::table {
        { "system", iris::ps2::NAMCO_SYSTEM_246 },
        { "name", "Kinnikuman Muscle Grand Prix 2 (KN2 Ver. A)" },
        { "gameid", "NM00040" },
        { "bootprog", "KI2LOAD" },
        { "bios", "r27v1602f.7d" },
        { "dongle", "kn2vera.ic002" },
        { "media", "kn2.chd" },
        { "media_type", iris::s2x6::acata::MEDIA_DVD }
    }},
    { "sbxc", toml::table {
        { "system", iris::ps2::NAMCO_SYSTEM_246 },
        { "name", "Sengoku Basara X Cross" },
        { "gameid", "NM00042" },
        { "bootprog", "BASLOAD" },
        { "bios", "r27v1602f.7d" },
        { "dongle", "bax1vera.ic002" },
        { "media", "bax1_dvd0.chd" },
        { "media_type", iris::s2x6::acata::MEDIA_DVD }
    }},
    { "fateulc", toml::table {
        { "system", iris::ps2::NAMCO_SYSTEM_246 },
        { "name", "Fate: Unlimited Codes (FUD1 ver. A)" },
        { "gameid", "NM00048" },
        { "bootprog", "FTELOAD" },
        { "bios", "r27v1602f.7d" },
        { "dongle", "fud1vera.ic002" },
        { "media", "fud-hdd0-a.chd" },
        { "media_type", iris::s2x6::acata::MEDIA_HDD }
    }},
    { "fateulcb", toml::table {
        { "system", iris::ps2::NAMCO_SYSTEM_246 },
        { "name", "Fate: Unlimited Codes (bootleg)" },
        { "gameid", "NM00048" },
        { "bootprog", "FTELOAD" },
        { "bios", "r27v1602f.7d" },
        { "dongle", "fates-dongle.bin" },
        { "media", "fateulcb.chd" },
        { "media_type", iris::s2x6::acata::MEDIA_HDD }
    }},
    { "cobrata", toml::table {
        { "system", iris::ps2::NAMCO_SYSTEM_256 },
        { "name", "Cobra: The Arcade (CBR1 Ver. B)" },
        { "gameid", "NM00021" },
        { "bootprog", "CBRLOAD" },
        { "bios", "r27v1602f.8g" },
        { "dongle", "cbr1verb.ic002" },
        { "media", "cbr1-ha.chd" },
        { "media_type", iris::s2x6::acata::MEDIA_HDD },
        { "jvs_mode", iris::s2x6::acjv::MODE_LIGHTGUN },
        { "gun_trigger", iris::s2x6::acjv::BTN_LEFT },
        { "gun_pedal", iris::s2x6::acjv::BTN_3 },
        { "gun_board", iris::s2x6::acjv::GUN_BOARD_CLASSIC },
        { "gun_sensor", iris::s2x6::acjv::BTN_RIGHT }
    }},
    { "taiko7", toml::table {
        { "system", iris::ps2::NAMCO_SYSTEM_256 },
        { "name", "Taiko no Tatsujin 7 (TK71-NA-A)" },
        { "gameid", "NM00023" },
        { "bootprog", "TA7LOAD" },
        { "bios", "r27v1602f.8g" },
        { "dongle", "tk71.ic002" },
        { "media", "tk71dvd0.chd" },
        { "media_type", iris::s2x6::acata::MEDIA_DVD }
    }},
    { "taiko8", toml::table {
        { "system", iris::ps2::NAMCO_SYSTEM_256 },
        { "name", "Taiko no Tatsujin 8 (TK8100-1-NA-A)" },
        { "gameid", "NM00033" },
        { "bootprog", "TA8LOAD" },
        { "bios", "r27v1602f.8g" },
        { "dongle", "tk81001-na-a.ic002" },
        { "media", "tk8100-1-na-dvd0-a.chd" },
        { "media_type", iris::s2x6::acata::MEDIA_DVD }
    }},
    { "minnadk", toml::table {
        { "system", iris::ps2::NAMCO_SYSTEM_256 },
        { "name", "Minna de Kitaeru Zenno Training (Ver. 1.50)" },
        { "gameid", "NM00036" },
        { "bootprog", "NTRLOAD" },
        { "bios", "r27v1602f.8g" },
        { "dongle", "nm00036_znt100-1-st-a.bin" },
        { "media", "znt150-1-na-dvd0-b.chd" },
        { "media_type", iris::s2x6::acata::MEDIA_DVD }
    }},
    { "acedriv3", toml::table {
        { "system", iris::ps2::NAMCO_SYSTEM_256 },
        { "name", "Ace Driver 3: Final Turn" },
        { "gameid", "NM00047" },
        { "bootprog", "NRALOAD" },
        { "jvs_mode", iris::s2x6::acjv::MODE_DRIVE },
        { "bios", "r27v1602f.8g" },
        { "dongle", "adt1005-na-a.ic002" },
        { "media", "adt1005-na-hdd0a.chd" },
        { "media_type", iris::s2x6::acata::MEDIA_HDD }
    }},
    { "tekken51", toml::table {
        { "system", iris::ps2::NAMCO_SYSTEM_256 },
        { "name", "Tekken 5.1 (TE51 Ver. B)" },
        { "gameid", "NM00019" },
        { "bootprog", "TK5LOAD" },
        { "bios", "r27v1602f.8g" },
        { "dongle", "te51verb.ic002" },
        { "media", "te51-dvd0.chd" },
        { "media_type", iris::s2x6::acata::MEDIA_DVD }
    }},
    { "tekken51b", toml::table {
        { "system", iris::ps2::NAMCO_SYSTEM_256 },
        { "name", "Tekken 5.1 (TE53 Ver. B)" },
        { "gameid", "NM00019" },
        { "bootprog", "TK5LOAD" },
        { "bios", "r27v1602f.8g" },
        { "dongle", "te53verb.ic002" },
        { "media", "te51-dvd0.chd" },
        { "media_type", iris::s2x6::acata::MEDIA_DVD }
    }},
    { "tekken5d", toml::table {
        { "system", iris::ps2::NAMCO_SYSTEM_256 },
        { "name", "Tekken 5 Dark Resurrection (TED1 Ver. A)" },
        { "gameid", "NM00026" },
        { "bootprog", "T55LOAD" },
        { "bios", "r27v1602f.8g" },
        { "dongle", "ted1vera.ic002" },
        { "media", "ted1dvd0b.chd" },
        { "media_type", iris::s2x6::acata::MEDIA_DVD }
    }},
    { "superdbz", toml::table {
        { "system", iris::ps2::NAMCO_SYSTEM_256 },
        { "name", "Super Dragon Ball Z (DB1 Ver. B)" },
        { "gameid", "NM00027" },
        { "bootprog", "DBALOAD" },
        { "bios", "r27v1602f.8g" },
        { "dongle", "db1verb.ic002" },
        { "media", "db1.chd" },
        { "media_type", iris::s2x6::acata::MEDIA_DVD }
    }},
    { "kinniku", toml::table {
        { "system", iris::ps2::NAMCO_SYSTEM_256 },
        { "name", "Kinnikuman Muscle Grand Prix (KN1 Ver. A)" },
        { "gameid", "NM00029" },
        { "bootprog", "KINLOAD" },
        { "bios", "r27v1602f.8g" },
        { "dongle", "kn1vera.ic002" },
        { "media", "kn1-b.chd" },
        { "media_type", iris::s2x6::acata::MEDIA_DVD }
    }},
    { "taiko9", toml::table {
        { "system", iris::ps2::NAMCO_SYSTEM_256 },
        { "name", "Taiko no Tatsujin 9 (TK91001-NA-A)" },
        { "gameid", "NM00038" },
        { "bootprog", "TA9LOAD" },
        { "bios", "r27v1602f.8g" },
        { "dongle", "tk91001-na-a.ic002" },
        { "media", "tk9100-1-na-dvd0-a.chd" },
        { "media_type", iris::s2x6::acata::MEDIA_DVD }
    }},
    { "yuyuhaku", toml::table {
        { "system", iris::ps2::NAMCO_SYSTEM_256 },
        { "name", "The Battle of Yu Yu Hakusho: Shitou! Ankoku Bujutsukai!" },
        { "gameid", "NM00035" },
        { "bootprog", "YUULOAD" },
        { "bios", "r27v1602f.8g" },
        { "dongle", "dongle.bin" },
        { "media", "yh1.chd" },
        { "media_type", iris::s2x6::acata::MEDIA_DVD }
    }},
    { "motogp", toml::table {
        { "system", iris::ps2::NAMCO_SYSTEM_256 },
        { "name", "MotoGP (MGP1004-NA-B)" },
        { "gameid", "NM00039" },
        { "bootprog", "MGPLOAD" },
        { "jvs_mode", iris::s2x6::acjv::MODE_DRIVE },
        { "bios", "r27v1602f.8g" },
        { "dongle", "mgp1004-na-b.ic002" },
        { "media", "mgp1004-na-hdd0-a.chd" },
        { "media_type", iris::s2x6::acata::MEDIA_HDD }
    }},
    { "taiko10", toml::table {
        { "system", iris::ps2::NAMCO_SYSTEM_256 },
        { "name", "Taiko no Tatsujin 10 (T101001-NA-A)" },
        { "gameid", "NM00041" },
        { "bootprog", "T10LOAD" },
        { "bios", "r27v1602f.8g" },
        { "dongle", "t101001-na-a.ic002" },
        { "media", "tk10100-1-na-dvd0-a.chd" },
        { "media_type", iris::s2x6::acata::MEDIA_DVD }
    }},
    { "taiko11", toml::table {
        { "system", iris::ps2::NAMCO_SYSTEM_256 },
        { "name", "Taiko no Tatsujin 11 (T111001-NA-A)" },
        { "gameid", "NM00044" },
        { "bootprog", "T11LOAD" },
        { "bios", "r27v1602f.8g" },
        { "dongle", "t111001-na-a.ic002" },
        { "media", "t11100-1-na-dvd0-a.chd" },
        { "media_type", iris::s2x6::acata::MEDIA_DVD }
    }},
    { "gdvsgd", toml::table {
        { "system", iris::ps2::NAMCO_SYSTEM_256 },
        { "name", "Gundam vs. Gundam (GVS1 Ver. A)" },
        { "gameid", "NM00043" },
        { "bootprog", "GDNLOAD" },
        { "bios", "r27v1602f.8g" },
        { "dongle", "gvs1vera.ic002" },
        { "media", "gvs1dvd0b.chd" },
        { "media_type", iris::s2x6::acata::MEDIA_DVD }
    }},
    { "gdvsgdnx", toml::table {
        { "system", iris::ps2::NAMCO_SYSTEM_256 },
        { "name", "Gundam vs. Gundam Next" },
        { "gameid", "NM00052" },
        { "bootprog", "GNXLOAD" },
        { "bios", "r27v1602f.8g" },
        { "dongle", "gnx1001-na-a.ic002" },
        { "media", "gnx100-1na-a.chd" },
        { "media_type", iris::s2x6::acata::MEDIA_HDD }
    }},
    { "timecrs4", toml::table {
        { "system", iris::ps2::NAMCO_SYSTEM_SUPER_256 },
        { "name", "Time Crisis 4 (World, TSF1002-NA-A)" },
        { "gameid", "NM00032" },
        { "bootprog", "TC4LOAD" },
        { "bios", "r27v1602f.8g" },
        { "dongle", "tsf1002-na-a.ic002" },
        { "media", "tsf1-ha.chd" },
        { "media_type", iris::s2x6::acata::MEDIA_HDD },
        { "jvs_mode", iris::s2x6::acjv::MODE_LIGHTGUN },
        { "gun_trigger", iris::s2x6::acjv::BTN_LEFT },
        { "gun_pedal", iris::s2x6::acjv::BTN_3 },
        { "gun_board", iris::s2x6::acjv::GUN_BOARD_SIDE_SWITCH },
        { "gun_sensor", iris::s2x6::acjv::BTN_RIGHT }
    }},
    { "timecrs4j", toml::table {
        { "system", iris::ps2::NAMCO_SYSTEM_SUPER_256 },
        { "name", "Time Crisis 4 (Japan, TSF1001-NA-A)" },
        { "gameid", "NM00032" },
        { "bootprog", "TC4LOAD" },
        { "bios", "r27v1602f.8g" },
        { "dongle", "tsf1001-na-a.ic002" },
        { "media", "tsf1-ha.chd" },
        { "media_type", iris::s2x6::acata::MEDIA_HDD },
        { "jvs_mode", iris::s2x6::acjv::MODE_LIGHTGUN },
        { "gun_trigger", iris::s2x6::acjv::BTN_LEFT },
        { "gun_pedal", iris::s2x6::acjv::BTN_3 },
        { "gun_board", iris::s2x6::acjv::GUN_BOARD_SIDE_SWITCH },
        { "gun_sensor", iris::s2x6::acjv::BTN_RIGHT }
    }}
};
