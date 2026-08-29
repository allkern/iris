#include <new>
#include <cctype>
#include <ctime>

#include "cdvd.hpp"

namespace iris::cdvd {

static void nvram_writeback(Cdvd* cdvd);

NvramLayout g_spc970_layout = {
    .bios_version = 0x00000000,
    .config0_offset = 0x00000280,
    .config1_offset = 0x00000300,
    .config2_offset = 0x00000200,
    .console_id_offset = 0x000001C8,
    .ilink_id_offset = 0x000001C0,
    .modelnum_offset = 0x000001A0,
    .regparams_offset = 0x00000180,
    .mac_offset = 0x00000198
};

NvramLayout g_dragon_layout = {
    .bios_version = 0x00000146,
    .config0_offset = 0x00000270,
    .config1_offset = 0x000002B0,
    .config2_offset = 0x00000200,
    .console_id_offset = 0x000001F0,
    .ilink_id_offset = 0x000001E0,
    .modelnum_offset = 0x000001B0,
    .regparams_offset = 0x00000180,
    .mac_offset = 0x00000198
};

static const uint8_t itob_table[] = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
    0x08, 0x09, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15,
    0x16, 0x17, 0x18, 0x19, 0x20, 0x21, 0x22, 0x23,
    0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x30, 0x31,
    0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39,
    0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47,
    0x48, 0x49, 0x50, 0x51, 0x52, 0x53, 0x54, 0x55,
    0x56, 0x57, 0x58, 0x59, 0x60, 0x61, 0x62, 0x63,
    0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x70, 0x71,
    0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79,
    0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,
    0x88, 0x89, 0x90, 0x91, 0x92, 0x93, 0x94, 0x95,
    0x96, 0x97, 0x98, 0x99, 0xa0, 0xa1, 0xa2, 0xa3,
    0xa4, 0xa5, 0xa6, 0xa7, 0xa8, 0xa9, 0xb0, 0xb1,
    0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7, 0xb8, 0xb9,
    0xc0, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7,
    0xc8, 0xc9, 0xd0, 0xd1, 0xd2, 0xd3, 0xd4, 0xd5,
    0xd6, 0xd7, 0xd8, 0xd9, 0xe0, 0xe1, 0xe2, 0xe3,
    0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xf0, 0xf1,
    0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9,
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
    0x08, 0x09, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15,
    0x16, 0x17, 0x18, 0x19, 0x20, 0x21, 0x22, 0x23,
    0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x30, 0x31,
    0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39,
    0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47,
    0x48, 0x49, 0x50, 0x51, 0x52, 0x53, 0x54, 0x55,
    0x56, 0x57, 0x58, 0x59, 0x60, 0x61, 0x62, 0x63,
    0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x70, 0x71,
    0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79,
    0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,
    0x88, 0x89, 0x90, 0x91, 0x92, 0x93, 0x94, 0x95,
};

static inline const char* get_n_command_name(uint8_t cmd) {
    switch (cmd) {
        case 0x00: return "nop";
        case 0x01: return "nop_sync";
        case 0x02: return "standby";
        case 0x03: return "stop";
        case 0x04: return "pause";
        case 0x05: return "seek";
        case 0x06: return "read_cd";
        case 0x07: return "read_cdda";
        case 0x08: return "read_dvd";
        case 0x09: return "get_toc";
        case 0x0c: return "read_key";
    }

    return "<unknown>";
}

static inline const char* get_type_name(int type) {
    switch (type) {
        case iop::disc::CDVD_DISC_NO_DISC: return "No disc";
        case iop::disc::CDVD_DISC_DETECTING: return "Detecting";
        case iop::disc::CDVD_DISC_DETECTING_CD: return "Detecting CD";
        case iop::disc::CDVD_DISC_DETECTING_DVD: return "Detecting DVD";
        case iop::disc::CDVD_DISC_DETECTING_DL_DVD: return "Detecting Dual-layer DVD";
        case iop::disc::CDVD_DISC_PSX_CD: return "PlayStation CD";
        case iop::disc::CDVD_DISC_PSX_CDDA: return "PlayStation CDDA";
        case iop::disc::CDVD_DISC_PS2_CD: return "PlayStation 2 CD";
        case iop::disc::CDVD_DISC_PS2_CDDA: return "PlayStation 2 CDDA";
        case iop::disc::CDVD_DISC_PS2_DVD: return "PlayStation 2 DVD";
        case iop::disc::CDVD_DISC_CDDA: return "CD Audio";
        case iop::disc::CDVD_DISC_DVD_VIDEO: return "DVD Video";
        case iop::disc::CDVD_DISC_INVALID: return "Invalid";
    }

    return "Unknown";
}

static inline int is_dual_layer(Cdvd* cdvd) {
    return iop::disc::get_volume_lba(cdvd->disc, 1);
}

static inline void set_busy(Cdvd* cdvd) {
    cdvd->n_stat |= N_STATUS_BUSY;
    cdvd->n_stat &= ~N_STATUS_READY;
}

static inline void set_ready(Cdvd* cdvd) {
    cdvd->n_stat &= ~N_STATUS_BUSY;
    cdvd->n_stat |= N_STATUS_READY;
}

static inline void init_s_fifo(Cdvd* cdvd, int size) {
    if (cdvd->s_fifo)
        free(cdvd->s_fifo);

    cdvd->s_fifo_size = size;
    cdvd->s_fifo_index = 0;
    cdvd->s_fifo = (uint8_t *)malloc(cdvd->s_fifo_size);
    cdvd->s_stat &= ~0x40;
}

static inline void s_read_subq(Cdvd* cdvd) {
    init_s_fifo(cdvd, 11);

    int track = iop::disc::get_track_number(cdvd->disc, cdvd->read_lba);

    iop::disc::TrackInfo info;
    iop::disc::get_track_info(cdvd->disc, track, &info);

    int track_lba = (cdvd->read_lba + 150) - info.lba;

    int track_mm = track_lba / (60 * 75);
    int track_ss = (track_lba % (60 * 75)) / 75;
    int track_ff = (track_lba % (60 * 75)) % 75;
    int abs_mm = (cdvd->read_lba + 150) / (60 * 75);
    int abs_ss = ((cdvd->read_lba + 150) % (60 * 75)) / 75;
    int abs_ff = ((cdvd->read_lba + 150) % (60 * 75)) % 75;

    iris_debug(cdvd, "S subq read: track={} trk {}:{}:{} abs {}:{}:{}", track,
        track_mm, track_ss, track_ff,
        abs_mm, abs_ss, abs_ff);

    // Note: From PCSX2
    //       the formatted subq command returns: 
    //       control/adr, track, index, trk min, trk sec, trk frm, 0x00, abs min, abs sec, abs frm
    memset(&cdvd->s_fifo[0], 0, 11);

    cdvd->s_fifo[0] = 0x41; // control/adr
    cdvd->s_fifo[1] = track; // track
    cdvd->s_fifo[2] = 0x01; // index
    cdvd->s_fifo[3] = itob_table[track_mm]; // trk min
    cdvd->s_fifo[4] = itob_table[track_ss]; // trk sec
    cdvd->s_fifo[5] = itob_table[track_ff]; // trk frm
    cdvd->s_fifo[6] = 0x00;
    cdvd->s_fifo[7] = itob_table[abs_mm]; // abs min
    cdvd->s_fifo[8] = itob_table[abs_ss]; // abs sec
    cdvd->s_fifo[9] = itob_table[abs_ff]; // abs frm

    // To-do: This doesn't work for whatever reason, even though
    //        we're returning correct data, the CD player just
    //        won't actually display the current time.
}
static inline void s_mechacon_cmd(Cdvd* cdvd) {
    switch (cdvd->s_params[0]) {
        case 0x00: {
            init_s_fifo(cdvd, 4);

            cdvd->s_fifo[0] = 0x03;
            cdvd->s_fifo[1] = 0x06;
            cdvd->s_fifo[2] = 0x02;
            cdvd->s_fifo[3] = 0x00;
        } break;

        case 0x90: {
            init_s_fifo(cdvd, 1);

            cdvd->s_fifo[0] = 0x00;
        } break;

        case 0xef: {
            init_s_fifo(cdvd, 3);

            cdvd->s_fifo[0] = 0x00;
            cdvd->s_fifo[1] = 0x0f;
            cdvd->s_fifo[2] = 0x05;
        } break;

        // sceCdReadRenewalDate (sent by PSX DESR BIOS)
        case 0xfd: {
            init_s_fifo(cdvd, 6);

            cdvd->s_fifo[0] = 0;
            cdvd->s_fifo[1] = 0x04; //year
            cdvd->s_fifo[2] = 0x12; //month
            cdvd->s_fifo[3] = 0x10; //day
            cdvd->s_fifo[4] = 0x01; //hour
            cdvd->s_fifo[5] = 0x30; //min
        } break;

        default: {
            iris_warning(cdvd, "Unknown S subcommand {:02x}", cdvd->s_params[0]);

            init_s_fifo(cdvd, 1);

            cdvd->s_fifo[0] = 0;
        } break;
    }
}
static inline void s_update_sticky_flags(Cdvd* cdvd) {
    init_s_fifo(cdvd, 1);

    cdvd->s_fifo[0] = 0;

    cdvd->sticky_status = cdvd->status;
}
static inline void s_read_rtc(Cdvd* cdvd) {
    time_t t = time(NULL);

    struct tm tm = *localtime(&t);

    init_s_fifo(cdvd, 8);

    cdvd->s_fifo[0] = 0;
    cdvd->s_fifo[1] = itob_table[tm.tm_sec];
    cdvd->s_fifo[2] = itob_table[tm.tm_min];
    cdvd->s_fifo[3] = itob_table[tm.tm_hour];
    cdvd->s_fifo[4] = 0;
    cdvd->s_fifo[5] = itob_table[tm.tm_mday];
    cdvd->s_fifo[6] = itob_table[tm.tm_mon + 1];
    cdvd->s_fifo[7] = itob_table[tm.tm_year - 100];
}
static inline void s_write_rtc(Cdvd* cdvd) {
    // iris_debug(cdvd, "write_rtc");
    init_s_fifo(cdvd, 1);

    cdvd->s_fifo[0] = 0;
}
static inline void s_read_nvram(Cdvd* cdvd) {
    uint16_t addr = *(uint16_t*)&cdvd->s_params[0];

    init_s_fifo(cdvd, 2);

    cdvd->s_fifo[0] = cdvd->nvram[((addr << 1) + 0) & 0x3ff];
    cdvd->s_fifo[1] = cdvd->nvram[((addr << 1) + 1) & 0x3ff];

    // iris_debug(cdvd, "read_nvram word={:04x} (byte@{:04x}) -> {:02x} {:02x}", //     addr, (addr << 1) & 0x3ff, cdvd->s_fifo[0], cdvd->s_fifo[1]);
}
static inline void s_write_nvram(Cdvd* cdvd) {
    uint16_t addr = *(uint16_t*)&cdvd->s_params[0];

    init_s_fifo(cdvd, 1);

    cdvd->nvram[((addr << 1) + 0) & 0x3ff] = cdvd->s_params[2];
    cdvd->nvram[((addr << 1) + 1) & 0x3ff] = cdvd->s_params[3];

    // iris_debug(cdvd, "write_nvram word={:04x} (byte@{:04x}) <- {:02x} {:02x}", //     addr, (addr << 1) & 0x3ff, cdvd->s_params[2], cdvd->s_params[3]);

    nvram_writeback(cdvd);

    cdvd->s_fifo[0] = 0;
}
static inline void s_read_ilink_id(Cdvd* cdvd) {
    init_s_fifo(cdvd, 9);

    static const uint8_t dummy[8] = {
        0xac, 0xff, 0xff, 0xff, 0xff, 0xb9, 0x86, 0x00
    };

    cdvd->s_fifo[0] = 0;

    const uint8_t* id = &cdvd->nvram[cdvd->layout.ilink_id_offset];

    int zeros = 0;
    int ones = 0;

    for (int i = 0; i < 8; i++) {
        zeros += id[i] == 0x00;
        ones += id[i] == 0xff;
    }

    int blank = zeros == 8 || ones == 8;

    memcpy(&cdvd->s_fifo[1], blank ? dummy : id, 8);

    // iris_debug(cdvd, "read_ilink_id -> {:02x}{:02x}{:02x}{:02x}{:02x}{:02x}{:02x}{:02x} (nvram={})", //     cdvd->s_fifo[1], cdvd->s_fifo[2], cdvd->s_fifo[3], cdvd->s_fifo[4],
    //     cdvd->s_fifo[5], cdvd->s_fifo[6], cdvd->s_fifo[7], cdvd->s_fifo[8], have);
}
static inline void s_ctrl_audio_digital_out(Cdvd* cdvd) {
    init_s_fifo(cdvd, 1);

    cdvd->s_fifo[0] = 0;
}
static inline void s_forbid_dvd(Cdvd* cdvd) {
    init_s_fifo(cdvd, 1);

    cdvd->s_fifo[0] = 5;
}
static inline void s_write_ilink_id(Cdvd* cdvd) {
    init_s_fifo(cdvd, 1);

    memcpy(&cdvd->nvram[cdvd->layout.ilink_id_offset], cdvd->s_params, 8);

    cdvd->s_fifo[0] = 0;
}

static inline void s_read_mac(Cdvd* cdvd) {
    init_s_fifo(cdvd, 9);

    cdvd->s_fifo[0] = 0;

    memcpy(&cdvd->s_fifo[1], &cdvd->nvram[cdvd->layout.mac_offset], 8);
}

static inline void s_write_mac(Cdvd* cdvd) {
    init_s_fifo(cdvd, 1);

    memcpy(&cdvd->nvram[cdvd->layout.mac_offset], cdvd->s_params, 8);

    cdvd->s_fifo[0] = 0;
}

static inline void s_write_region_params(Cdvd* cdvd) {
    init_s_fifo(cdvd, 1);

    memcpy(&cdvd->nvram[cdvd->layout.regparams_offset], &cdvd->s_params[2], 8);

    cdvd->s_fifo[0] = 0;
}

static inline void s_read_model(Cdvd* cdvd) {
    init_s_fifo(cdvd, 9);

    int offset = cdvd->layout.modelnum_offset + cdvd->s_params[0];

    cdvd->s_fifo[0] = 0; // status: success (was left uninitialized)
    memcpy(&cdvd->s_fifo[1], &cdvd->nvram[offset], 8);

    // iris_debug(cdvd, "read_model[{:02x}] @{:04x} -> {:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x} '{}{}{}{}{}{}{}{}'", //     cdvd->s_params[0], offset,
    //     cdvd->s_fifo[1], cdvd->s_fifo[2], cdvd->s_fifo[3], cdvd->s_fifo[4],
    //     cdvd->s_fifo[5], cdvd->s_fifo[6], cdvd->s_fifo[7], cdvd->s_fifo[8],
    //     cdvd->s_fifo[1], cdvd->s_fifo[2], cdvd->s_fifo[3], cdvd->s_fifo[4],
    //     cdvd->s_fifo[5], cdvd->s_fifo[6], cdvd->s_fifo[7], cdvd->s_fifo[8]);
}
static inline void s_write_model(Cdvd* cdvd) {
    init_s_fifo(cdvd, 1);

    memcpy(&cdvd->nvram[cdvd->layout.modelnum_offset + cdvd->s_params[0]], &cdvd->s_params[1], 8);

    cdvd->s_fifo[0] = 0;
}

static inline void s_certify_boot(Cdvd* cdvd) {
    init_s_fifo(cdvd, 1);

    cdvd->s_fifo[0] = 1;
}
static inline void s_cancel_pwoff_ready(Cdvd* cdvd) {
    init_s_fifo(cdvd, 1);

    cdvd->s_fifo[0] = 0;
}
static inline void s_blue_led_ctl(Cdvd* cdvd) {
    init_s_fifo(cdvd, 1);

    cdvd->s_fifo[0] = 0;
}
static inline void s_read_wakeup_time(Cdvd* cdvd) {
    init_s_fifo(cdvd, 10);

    for (int i = 0; i < 10; i++)
        cdvd->s_fifo[i] = 0;
}
static inline void s_rc_bypass_ctrl(Cdvd* cdvd) {
    init_s_fifo(cdvd, 1);

    cdvd->s_fifo[0] = 0;
}
static inline void s_open_config(Cdvd* cdvd) {
    init_s_fifo(cdvd, 1);

    cdvd->config_rw = cdvd->s_params[0];
    cdvd->config_offset = cdvd->s_params[1];
    cdvd->config_numblocks = cdvd->s_params[2];
    cdvd->config_block_index = 0;

    cdvd->s_fifo[0] = 0;
}
static inline void s_read_config(Cdvd* cdvd) {
    init_s_fifo(cdvd, 16);

    int offset = 0;

    switch (cdvd->config_offset) {
        case 0: offset = cdvd->layout.config0_offset; break;
        case 1: offset = cdvd->layout.config1_offset; break;
        case 2: offset = cdvd->layout.config2_offset; break;

        default: offset = cdvd->layout.config1_offset; break;
    }

    int block = cdvd->config_block_index++;
    offset += block * 16;

    memcpy(cdvd->s_fifo, &cdvd->nvram[offset], 16);

    // iris_debug(cdvd, "read_config cfg{} blk{} @{:04x}:", cdvd->config_offset, block, offset);

    // for (int i = 0; i < 16; i++)
    //     iris_debug(cdvd, "{:02x}", cdvd->s_fifo[i]);

    // iris_debug(cdvd, "");
}
static inline void s_write_config(Cdvd* cdvd) {
    init_s_fifo(cdvd, 1);

    int offset = 0;

    switch (cdvd->config_offset) {
        case 0: offset = cdvd->layout.config0_offset; break;
        case 1: offset = cdvd->layout.config1_offset; break;
        case 2: offset = cdvd->layout.config2_offset; break;
        default: offset = cdvd->layout.config1_offset; break;
    }

    int block = cdvd->config_block_index++;

    offset += block * 16;

    memcpy(&cdvd->nvram[offset], cdvd->s_params, 16);

    // iris_debug(cdvd, "write_config cfg{} blk{} @{:04x}:", cdvd->config_offset, block, offset);

    // for (int i = 0; i < 16; i++)
    //     iris_debug(cdvd, "{:02x}", cdvd->s_params[i]);

    // iris_debug(cdvd, "");

    nvram_writeback(cdvd);

    cdvd->s_fifo[0] = 0;
}
static inline void s_close_config(Cdvd* cdvd) {
    init_s_fifo(cdvd, 1);

    cdvd->s_fifo[0] = 0;
}
static inline uint16_t get_u16_le(const uint8_t* p, size_t ofs) {
    return (uint16_t)(p[ofs] | ((uint16_t)p[ofs + 1] << 8));
}

static inline void mg_fail(Cdvd* cdvd) {
    init_s_fifo(cdvd, 1);
    cdvd->s_fifo[0] = 0x80;
}

static int mg_BIToffset(const uint8_t* buffer) {
    int ofs = 0x20;
    uint16_t count = get_u16_le(buffer, 0x1A);
    uint16_t flags = get_u16_le(buffer, 0x18);

    for (int i = 0; i < count; i++)
        ofs += 0x10;

    if (flags & 1)
        ofs += buffer[ofs];

    if ((flags & 0xF000) == 0)
        ofs += 8;

    return ofs + 0x20;
}

static inline void mg_clear(Cdvd* cdvd) {
    cdvd->mg_size = 0;
    cdvd->mg_maxsize = 0;
    cdvd->mg_datatype = 0;
    memset(cdvd->mg_buffer, 0, sizeof(cdvd->mg_buffer));
    memset(cdvd->mg_kbit, 0, sizeof(cdvd->mg_kbit));
    memset(cdvd->mg_kcon, 0, sizeof(cdvd->mg_kcon));
}

static inline void s_mg_auth_80(Cdvd* cdvd) {
    iris_debug(cdvd, "mg: Auth 80");
    init_s_fifo(cdvd, 1);
    cdvd->mg_datatype = 0;
    cdvd->s_fifo[0] = 0;
}

static inline void s_mg_auth_81(Cdvd* cdvd) {
    iris_debug(cdvd, "mg: Auth 81");
    init_s_fifo(cdvd, 1);
    cdvd->mg_datatype = 0;
    cdvd->s_fifo[0] = 0;
}

static inline void s_mg_auth_82(Cdvd* cdvd) {
    iris_debug(cdvd, "mg: Auth 82");
    init_s_fifo(cdvd, 1);
    cdvd->s_fifo[0] = 0;
}

static inline void s_mg_auth_83(Cdvd* cdvd) {
    iris_debug(cdvd, "mg: Auth 83");
    init_s_fifo(cdvd, 1);
    cdvd->s_fifo[0] = 0;
}

static inline void s_mg_auth_84(Cdvd* cdvd) {
    iris_debug(cdvd, "mg: Auth 84");
    init_s_fifo(cdvd, 13);
    cdvd->s_fifo[0] = 0;

    cdvd->s_fifo[1] = 0x21;
    cdvd->s_fifo[2] = 0xdc;
    cdvd->s_fifo[3] = 0x31;
    cdvd->s_fifo[4] = 0x96;
    cdvd->s_fifo[5] = 0xce;
    cdvd->s_fifo[6] = 0x72;
    cdvd->s_fifo[7] = 0xe0;
    cdvd->s_fifo[8] = 0xc8;
    cdvd->s_fifo[9] = 0x69;
    cdvd->s_fifo[10] = 0xda;
    cdvd->s_fifo[11] = 0x34;
    cdvd->s_fifo[12] = 0x9b;
}

static inline void s_mg_auth_85(Cdvd* cdvd) {
    iris_debug(cdvd, "mg: Auth 85");
    init_s_fifo(cdvd, 13);
    cdvd->s_fifo[0] = 0;

    cdvd->s_fifo[1] = 0xeb;
    cdvd->s_fifo[2] = 0x01;
    cdvd->s_fifo[3] = 0xc7;
    cdvd->s_fifo[4] = 0xa9;
    cdvd->s_fifo[5] = 0x3f;
    cdvd->s_fifo[6] = 0x9c;
    cdvd->s_fifo[7] = 0x5b;
    cdvd->s_fifo[8] = 0x19;
    cdvd->s_fifo[9] = 0x31;
    cdvd->s_fifo[10] = 0xa0;
    cdvd->s_fifo[11] = 0xb3;
    cdvd->s_fifo[12] = 0xa3;
}

static inline void s_mg_auth_86(Cdvd* cdvd) {
    iris_debug(cdvd, "mg: Auth 86");
    init_s_fifo(cdvd, 1);
    cdvd->s_fifo[0] = 0;
}

static inline void s_mg_auth_87(Cdvd* cdvd) {
    iris_debug(cdvd, "mg: Auth 87");
    init_s_fifo(cdvd, 1);
    cdvd->s_fifo[0] = 0;
}

static inline void s_mg_auth_88(Cdvd* cdvd) {
    iris_debug(cdvd, "mg: Auth 88");

    init_s_fifo(cdvd, 1);

    if (cdvd->mg_datatype == 1) {
        int bit_ofs;

        if (cdvd->mg_maxsize != cdvd->mg_size ||
            cdvd->mg_size < 0x20 ||
            cdvd->mg_size != get_u16_le(cdvd->mg_buffer, 0x14)) {
            cdvd->s_fifo[0] = 0x80;
            return;
        }

        bit_ofs = mg_BIToffset(cdvd->mg_buffer);

        if (bit_ofs < 0x20 || (size_t)bit_ofs > sizeof(cdvd->mg_buffer)) {
            cdvd->s_fifo[0] = 0x80;
            return;
        }

        size_t kbit_ofs = (size_t)bit_ofs - 0x20;
        size_t kcon_ofs = (size_t)bit_ofs - 0x10;

        memcpy(cdvd->mg_kbit, &cdvd->mg_buffer[kbit_ofs], 16);
        memcpy(cdvd->mg_kcon, &cdvd->mg_buffer[kcon_ofs], 16);

        if ((cdvd->mg_buffer[bit_ofs + 5] || cdvd->mg_buffer[bit_ofs + 6] || cdvd->mg_buffer[bit_ofs + 7]) ||
            ((uint16_t)cdvd->mg_buffer[bit_ofs + 4] * 16u + (uint16_t)bit_ofs + 8u + 16u
                != get_u16_le(cdvd->mg_buffer, 0x14))) {
            cdvd->s_fifo[0] = 0x80;
            return;
        }
    }

    cdvd->s_fifo[0] = 0;
}

static inline void s_mg_auth_8f(Cdvd* cdvd) {
    iris_debug(cdvd, "mg: Auth 8f");
    s_mg_auth_88(cdvd);
}

static inline void s_mg_write_data(Cdvd* cdvd) {
    iris_debug(cdvd, "mg: Write Data");
    init_s_fifo(cdvd, 1);

    if ((size_t)cdvd->mg_size + (size_t)cdvd->s_param_index > (size_t)cdvd->mg_maxsize) {
        cdvd->s_fifo[0] = 0x80;
        return;
    }

    if ((size_t)cdvd->mg_size + (size_t)cdvd->s_param_index > sizeof(cdvd->mg_buffer)) {
        cdvd->s_fifo[0] = 0x80;
        return;
    }

    memcpy(&cdvd->mg_buffer[cdvd->mg_size], cdvd->s_params, cdvd->s_param_index);
    cdvd->mg_size = (uint16_t)(cdvd->mg_size + cdvd->s_param_index);
    cdvd->s_fifo[0] = 0;
}
static inline void s_mg_read_data(Cdvd* cdvd) {
    iris_debug(cdvd, "mg: Read Data");

    uint16_t count = cdvd->mg_size;
    if (count > 16)
        count = 16;

    init_s_fifo(cdvd, count);

    memcpy(cdvd->s_fifo, cdvd->mg_buffer, count);

    cdvd->mg_size = (uint16_t)(cdvd->mg_size - count);
    memmove(cdvd->mg_buffer,
        cdvd->mg_buffer + count,
        cdvd->mg_size);
}

static inline void s_mg_write_hdr_start(Cdvd* cdvd) {
    iris_debug(cdvd, "mg: Write HDR Start");
    init_s_fifo(cdvd, 1);

    cdvd->mg_size = 0;
    cdvd->mg_datatype = 1;
    cdvd->mg_maxsize = (uint16_t)(cdvd->s_params[1] | ((uint16_t)cdvd->s_params[2] << 8));

    cdvd->s_fifo[0] = 0;
}

static inline void s_mg_read_bit_length(Cdvd* cdvd) {
    iris_debug(cdvd, "mg: Read Bit Length");
    init_s_fifo(cdvd, 3);

    int bit_ofs = mg_BIToffset(cdvd->mg_buffer);
    if (bit_ofs < 0) {
        cdvd->s_fifo[0] = 0x80;
        cdvd->s_fifo[1] = 0;
        cdvd->s_fifo[2] = 0;
        return;
    }

    size_t ofs = (size_t)bit_ofs;
    if (ofs > sizeof(cdvd->mg_buffer) - 5) {
        cdvd->s_fifo[0] = 0x80;
        cdvd->s_fifo[1] = 0;
        cdvd->s_fifo[2] = 0;
        return;
    }

    unsigned int blocks = cdvd->mg_buffer[ofs + 4];
    size_t copy_len = 8 + 16u * (size_t)blocks;

    if (copy_len > sizeof(cdvd->mg_buffer) - ofs) {
        cdvd->s_fifo[0] = 0x80;
        cdvd->s_fifo[1] = 0;
        cdvd->s_fifo[2] = 0;
        return;
    }

    memmove(&cdvd->mg_buffer[0], &cdvd->mg_buffer[ofs], copy_len);

    cdvd->mg_maxsize = 0;
    cdvd->mg_size = (uint16_t)(8 + 16u * cdvd->mg_buffer[4]);

    cdvd->s_fifo[0] = (cdvd->mg_datatype == 1) ? 0 : 0x80;
    cdvd->s_fifo[1] = (cdvd->mg_size >> 0) & 0xFF;
    cdvd->s_fifo[2] = (cdvd->mg_size >> 8) & 0xFF;
}

static inline void s_mg_write_datain_length(Cdvd* cdvd) {
    iris_debug(cdvd, "mg: Write Datain Length");
    init_s_fifo(cdvd, 1);
    cdvd->mg_size = 0;
    cdvd->mg_datatype = 0;
    cdvd->mg_maxsize = (uint16_t)(cdvd->s_params[0] | ((uint16_t)cdvd->s_params[1] << 8));
    cdvd->s_fifo[0] = 0;
}

static inline void s_mg_write_dataout_length(Cdvd* cdvd) {
    iris_debug(cdvd, "mg: Write Dataout Length");
    init_s_fifo(cdvd, 1);

    uint16_t want = (uint16_t)(cdvd->s_params[0] | ((uint16_t)cdvd->s_params[1] << 8));
    if (want == cdvd->mg_size && cdvd->mg_datatype == 0) {
        cdvd->mg_maxsize = 0;
        cdvd->s_fifo[0] = 0;
    }
    else {
        cdvd->s_fifo[0] = 0x80;
    }
}

static inline void s_mg_read_kbit(Cdvd* cdvd) {
    iris_debug(cdvd, "mg: Read KBit");
    init_s_fifo(cdvd, 9);
    cdvd->s_fifo[0] = 0;
    memcpy(&cdvd->s_fifo[1], cdvd->mg_kbit, 8);
}

static inline void s_mg_read_kbit2(Cdvd* cdvd) {
    iris_debug(cdvd, "mg: Read KBit2");
    init_s_fifo(cdvd, 9);
    cdvd->s_fifo[0] = 0;
    memcpy(&cdvd->s_fifo[1], cdvd->mg_kbit + 8, 8);
}

static inline void s_mg_read_kcon(Cdvd* cdvd) {
    iris_debug(cdvd, "mg: Read KCon");
    init_s_fifo(cdvd, 9);
    cdvd->s_fifo[0] = 0;
    memcpy(&cdvd->s_fifo[1], cdvd->mg_kcon, 8);
}

static inline void s_mg_read_kcon2(Cdvd* cdvd) {
    iris_debug(cdvd, "mg: Read KCon2");
    init_s_fifo(cdvd, 9);
    cdvd->s_fifo[0] = 0;
    memcpy(&cdvd->s_fifo[1], cdvd->mg_kcon + 8, 8);
}
static inline void s_get_region_params(Cdvd* cdvd) {
    init_s_fifo(cdvd, 15);

    for (int i = 5; i < 15; i++)
        cdvd->s_fifo[i] = 0;

    int offset = cdvd->layout.regparams_offset;

    cdvd->s_fifo[0] = 0; // status: success
    cdvd->s_fifo[1] = 1 << 3;
    cdvd->s_fifo[2] = 0;

    memcpy(&cdvd->s_fifo[3], &cdvd->nvram[offset], 8);

    iris_debug(cdvd, "region params @{:04x} zone={:02x} bytes={:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x}", offset, cdvd->s_fifo[1],
        cdvd->s_fifo[3], cdvd->s_fifo[4], cdvd->s_fifo[5], cdvd->s_fifo[6],
        cdvd->s_fifo[7], cdvd->s_fifo[8], cdvd->s_fifo[9], cdvd->s_fifo[10]);

    // This is basically what PCSX2 returns on a blank NVM/MEC file
    // cdvd->s_fifo[0] = 0;
    // cdvd->s_fifo[1] = 1 << 0x3; //MEC encryption zone
    // cdvd->s_fifo[2] = 0;
    // cdvd->s_fifo[3] = 0x80; //Region Params
    // cdvd->s_fifo[4] = 0x1;
}
static inline void s_remote2_read(Cdvd* cdvd) {
    init_s_fifo(cdvd, 5);

    cdvd->s_fifo[0] = 0x00;
    cdvd->s_fifo[1] = 0x14;
    cdvd->s_fifo[2] = 0x00;
    cdvd->s_fifo[3] = 0x00;
    cdvd->s_fifo[4] = 0x00;
}
static inline void s_remote2_6(Cdvd* cdvd) {
    init_s_fifo(cdvd, 3);

    cdvd->s_fifo[0] = 0;
    cdvd->s_fifo[1] = 1;
    cdvd->s_fifo[2] = 0;
}
static inline void s_auto_adjust_ctrl(Cdvd* cdvd) {
    init_s_fifo(cdvd, 1);

    cdvd->s_fifo[0] = 0;
}
static inline void s_notice_game_start(Cdvd* cdvd) {
    init_s_fifo(cdvd, 1);

    cdvd->s_fifo[0] = 0;
}
static inline void s_psx_unk_remote_2b(Cdvd* cdvd) {
    init_s_fifo(cdvd, 1);

    cdvd->s_fifo[0] = 0;
}
static inline void s_psx_unk_remote_2c(Cdvd* cdvd) {
    init_s_fifo(cdvd, 1);

    cdvd->s_fifo[0] = 0;
}
static inline void s_set_medium_removal(Cdvd* cdvd) {
    init_s_fifo(cdvd, 2);

    cdvd->s_fifo[0] = 0;
}
static inline void s_get_medium_removal(Cdvd* cdvd) {
    init_s_fifo(cdvd, 2);

    cdvd->s_fifo[0] = 0;
}

void handle_s_command(Cdvd* cdvd, uint8_t cmd) {
    cdvd->s_cmd = cmd;

#ifdef IRIS_ENABLE_MAGICGATE
    if (cmd >= 0x80 && cmd <= 0x98 && cdvd->mg_enabled && cdvd->mecha_keys.ready && cdvd->mecha_state) {
        uint8_t result[16] = {};
        int size = 0;

        if (mg::command(&cdvd->mecha_keys, cdvd->mecha_state, cmd, cdvd->s_params, cdvd->s_param_index, result, &size)) {
            // iris_debug(cdvd, "magicgate {:02x} params {} -> {} bytes, state {}, result {:03x}, err {:02x}",
            //     cmd, cdvd->s_param_index, size, cdvd->mecha_state->state,
            //     cdvd->mecha_state->result, cdvd->mecha_state->errorcode);

            init_s_fifo(cdvd, size);

            memcpy(cdvd->s_fifo, result, size);

            cdvd->s_param_index = 0;

            return;
        }
    }
#endif

    switch (cmd) {
        // Note: Used by CD player to get current playing position
        case 0x02: iris_debug(cdvd, "read_subq"); s_read_subq(cdvd); break;
        case 0x03: iris_debug(cdvd, "mechacon_cmd({:02x})", cdvd->s_params[0]); s_mechacon_cmd(cdvd); break;
        case 0x05: iris_debug(cdvd, "update_sticky_flags"); s_update_sticky_flags(cdvd); break;
        // case 0x06: iris_debug(cdvd, "tray_ctrl"); s_tray_ctrl(cdvd); break;
        case 0x08: iris_debug(cdvd, "read_rtc"); s_read_rtc(cdvd); break;
        case 0x09: iris_debug(cdvd, "write_rtc"); s_write_rtc(cdvd); break;
        case 0x0a: iris_debug(cdvd, "read_nvram"); s_read_nvram(cdvd); break;
        case 0x0b: iris_debug(cdvd, "write_nvram"); s_write_nvram(cdvd); break;
        // case 0x0f: iris_debug(cdvd, "power_off"); s_power_off(cdvd); break;
        case 0x12: iris_debug(cdvd, "read_ilink_id"); s_read_ilink_id(cdvd); break;
        case 0x13: iris_debug(cdvd, "write_ilink_id"); s_write_ilink_id(cdvd); break;

        // Note: Used by CD player
        case 0x14: iris_debug(cdvd, "ctrl_audio_digital_out"); s_ctrl_audio_digital_out(cdvd); break;
        case 0x15: iris_debug(cdvd, "forbid_dvd"); s_forbid_dvd(cdvd); break;
        case 0x16: iris_debug(cdvd, "auto_adjust_ctrl"); s_auto_adjust_ctrl(cdvd); break;
        case 0x17: iris_debug(cdvd, "read_model"); s_read_model(cdvd); break;
        case 0x18: iris_debug(cdvd, "write_model"); s_write_model(cdvd); break;
        case 0x1a: iris_debug(cdvd, "certify_boot"); s_certify_boot(cdvd); break;
        case 0x1b: iris_debug(cdvd, "cancel_pwoff_ready"); s_cancel_pwoff_ready(cdvd); break;

        // Used by Namco System 246 at boot
        case 0x1c: iris_debug(cdvd, "blue_led_ctl"); s_blue_led_ctl(cdvd); break;
        case 0x1e: iris_debug(cdvd, "remote2_read"); s_remote2_read(cdvd); break;

        // Used by the PS2 DVD player
        case 0x20: iris_debug(cdvd, "remote2_6"); s_remote2_6(cdvd); break;
        case 0x22: iris_debug(cdvd, "read_wakeup_time"); s_read_wakeup_time(cdvd); break;
        case 0x24: iris_debug(cdvd, "rc_bypass_ctrl"); s_rc_bypass_ctrl(cdvd); break;
        case 0x29: iris_debug(cdvd, "notice_game_start"); s_notice_game_start(cdvd); break;

        // No idea what this is, used by PSX DESR
        case 0x2b: iris_debug(cdvd, "psx_unk_remote_2b"); s_psx_unk_remote_2b(cdvd); break;
        case 0x2c: iris_debug(cdvd, "psx_unk_remote_2c"); s_psx_unk_remote_2c(cdvd); break;
        case 0x31: iris_debug(cdvd, "set_medium_removal"); s_set_medium_removal(cdvd); break;
        case 0x32: iris_debug(cdvd, "get_medium_removal"); s_get_medium_removal(cdvd); break;
        case 0x36: iris_debug(cdvd, "get_region_params"); s_get_region_params(cdvd); break;
        case 0x37: iris_debug(cdvd, "read_mac"); s_read_mac(cdvd); break;
        case 0x38: iris_debug(cdvd, "write_mac"); s_write_mac(cdvd); break;
        case 0x3e: iris_debug(cdvd, "write_region_params"); s_write_region_params(cdvd); break;
        case 0x40: iris_debug(cdvd, "open_config"); s_open_config(cdvd); break;
        case 0x41: iris_debug(cdvd, "read_config"); s_read_config(cdvd); break;
        case 0x42: iris_debug(cdvd, "write_config"); s_write_config(cdvd); break;
        case 0x43: iris_debug(cdvd, "close_config"); s_close_config(cdvd); break;
        case 0x80: iris_debug(cdvd, "mg_auth_80"); s_mg_auth_80(cdvd); break;
        case 0x81: iris_debug(cdvd, "mg_auth_81"); s_mg_auth_81(cdvd); break;
        case 0x82: iris_debug(cdvd, "mg_auth_82"); s_mg_auth_82(cdvd); break;
        case 0x83: iris_debug(cdvd, "mg_auth_83"); s_mg_auth_83(cdvd); break;
        case 0x84: iris_debug(cdvd, "mg_auth_84"); s_mg_auth_84(cdvd); break;
        case 0x85: iris_debug(cdvd, "mg_auth_85"); s_mg_auth_85(cdvd); break;
        case 0x86: iris_debug(cdvd, "mg_auth_86"); s_mg_auth_86(cdvd); break;
        case 0x87: iris_debug(cdvd, "mg_auth_87"); s_mg_auth_87(cdvd); break;
        case 0x88: iris_debug(cdvd, "mg_auth_88"); s_mg_auth_88(cdvd); break;
        case 0x8d: iris_debug(cdvd, "mg_write_data"); s_mg_write_data(cdvd); break;
        case 0x8E: iris_debug(cdvd, "mg_readdata"); s_mg_read_data(cdvd); break;
        case 0x8f: iris_debug(cdvd, "mg_auth_8f"); s_mg_auth_8f(cdvd); break;
        case 0x90: iris_debug(cdvd, "mg_write_hdr_start"); s_mg_write_hdr_start(cdvd); break;
        case 0x91: iris_debug(cdvd, "mg_read_bit_length"); s_mg_read_bit_length(cdvd); break;
        case 0x92: iris_debug(cdvd, "mg_write_datain_length"); s_mg_write_datain_length(cdvd); break;
        case 0x93: iris_debug(cdvd, "mg_write_dataout_length"); s_mg_write_dataout_length(cdvd); break;
        case 0x94: iris_debug(cdvd, "mg_read_kbit"); s_mg_read_kbit(cdvd); break;
        case 0x95: iris_debug(cdvd, "mg_read_kbit2"); s_mg_read_kbit2(cdvd); break;
        case 0x96: iris_debug(cdvd, "mg_read_kcon"); s_mg_read_kcon(cdvd); break;
        case 0x97: iris_debug(cdvd, "mg_read_kcon2"); s_mg_read_kcon2(cdvd); break;
        default: {
            iris_warning(cdvd, "Unknown S command {:02x}h ({} params)", cmd, cdvd->s_param_index);

            init_s_fifo(cdvd, 1);

            cdvd->s_fifo[0] = 0x80;
        } break;
    }

    cdvd->s_param_index = 0;
}

static inline void handle_s_param(Cdvd* cdvd, uint8_t param) {
    if (cdvd->s_param_index >= 16) {
        iris_debug(cdvd, "S parameter FIFO overflow");

        return;
    }

    cdvd->s_params[cdvd->s_param_index++] = param;
}

static inline uint8_t read_s_response(Cdvd* cdvd) {
    if (cdvd->s_fifo_index == cdvd->s_fifo_size) {
        return 0;
    }

    uint8_t data = cdvd->s_fifo[cdvd->s_fifo_index++];

    if (cdvd->s_fifo_index == cdvd->s_fifo_size)
        cdvd->s_stat |= 0x40;

    return data;
}

static inline long get_read_timing(Cdvd* cdvd, int dvd, int from) {
    long read_speed = dvd ? 4 * 1382400 : 24 * 153600;
    long block_timing = (36864000L * cdvd->read_size) / read_speed;
    long delta = cdvd->read_lba - from;
    long contiguous_cycles = block_timing * cdvd->read_count;

    long cycles = 0;

    if (!delta) {
        delta = 1;
    } else if (delta < 0) {
        delta = -delta;
    }

    if (delta < (dvd ? 16 : 8)) {
        // Small delta
        cycles = (block_timing * delta) + contiguous_cycles;
    } else if (delta < (dvd ? 16764 : 4371)) {
        // Fast seek: ~30ms
        cycles = ((36864000 / 1000) * 30) + contiguous_cycles;
    } else {
        // Full seek: ~100ms
        cycles = ((36864000 / 1000) * 100) + contiguous_cycles;
    }

    // Convert to EE cycles
    return cycles * 8;
}

static inline void set_status(Cdvd* cdvd, uint8_t data) {
    cdvd->status = data;
    cdvd->sticky_status |= data;
}

static inline void send_irq(Cdvd* cdvd) {
    iop::intc::irq(cdvd->hw.intc, iop::intc::CDVD);

    cdvd->i_stat |= 2;
}

void fetch_sector(Cdvd* cdvd) {
    memset(cdvd->buf, 0, 2352);

    switch (cdvd->read_size) {
        case CD_SS_2048:
        case CD_SS_2328: {
            iop::disc::read_sector(cdvd->disc, cdvd->buf, cdvd->read_lba++, iop::disc::DISC_SS_DATA);
        } break;
        case CD_SS_2352: {
            iop::disc::read_sector(cdvd->disc, cdvd->buf, cdvd->read_lba++, iop::disc::DISC_SS_RAW);
        } break;
        case CD_SS_2340: {
            // LBA -> MSF
            uint64_t a = cdvd->read_lba + 150;
            uint32_t m = a / 4500;

            a -= m * 4500;

            uint32_t s = a / 75;
            uint32_t f = a - (s * 75);

            // Fill in header
            cdvd->buf[0] = itob_table[m];
            cdvd->buf[1] = itob_table[s];
            cdvd->buf[2] = itob_table[f];
            cdvd->buf[3] = 1;

            // Write raw data at offset 12
            iop::disc::read_sector(cdvd->disc, cdvd->buf + 12, cdvd->read_lba++, iop::disc::DISC_SS_DATA);
        } break;
        case DVD_SS: {
            memset(cdvd->buf, 0, 2340);

            uint32_t lba, layer;

            if (cdvd->layer2_lba && (cdvd->read_lba >= cdvd->layer2_lba)) {
                layer = cdvd->read_lba >= cdvd->layer2_lba;
                lba = cdvd->read_lba - cdvd->layer2_lba + 0x30000;
            } else {
                layer = 0;
                lba = cdvd->read_lba + 0x30000;
            }

            cdvd->buf[0] = 0x20 | layer;
            cdvd->buf[1] = (lba >> 16) & 0xFF;
            cdvd->buf[2] = (lba >> 8) & 0xFF;
            cdvd->buf[3] = lba & 0xff;

            iop::disc::read_sector(cdvd->disc, cdvd->buf + 12, cdvd->read_lba++, iop::disc::DISC_SS_DATA);

            // for (int i = 0; i < 2064;) {
            //     for (int x = 0; x < 16; x++) {
            //         iris_debug(cdvd, "{:02x}", cdvd->buf[i+x]);
            //     }
    
            //     putchar('|');
    
            //     for (int x = 0; x < 16; x++) {
            //         iris_debug(cdvd, "{}", isprint(cdvd->buf[i+x]) ? cdvd->buf[i+x] : '.');
            //     }
    
            //     puts("|");
    
            //     i += 16;
            // }
        } break;
    }

    if (!cdvd->mecha_decode)
        return;

    uint8_t shift_amount = (cdvd->mecha_decode >> 4) & 7;
    int do_xor = (cdvd->mecha_decode) & 1;
    int do_shift = (cdvd->mecha_decode) & 2;

    for (int i = 0; i < cdvd->read_size; ++i) {
        if (do_xor) cdvd->buf[i] ^= cdvd->cdkey[4];
        if (do_shift) cdvd->buf[i] = (cdvd->buf[i] >> shift_amount) | (cdvd->buf[i] << (8 - shift_amount));
    }
}

void do_read(void* udata, int overshoot) {
    Cdvd* cdvd = (Cdvd*)udata;

    // Ugly hack!!
    // Some games will send
    if (!(cdvd->hw.dma->channels[iop::dma::CDVD].chcr & 0x1000000)) {
        // iris_debug(cdvd, "CDVD DMA not yet ready");

        scheduler::Event event;

        event.name = "CDVD Read";
        event.udata = cdvd;
        event.callback = do_read;
        event.cycles = 1000;

        scheduler::schedule(cdvd->hw.sched, event);

        set_status(cdvd, STATUS_READING);

        return;
    }

    // Fetch a sector
    fetch_sector(cdvd);

    // Send sector to DMA
    cdvd->buf_size = cdvd->read_size;
    cdvd->read_count--;

    // iris_debug(cdvd, "Sending a sector to DMA (left={})", cdvd->read_count);

    iop::dma::handle_cdvd_transfer(cdvd->hw.dma);

    if (cdvd->read_count) {
        scheduler::Event event;

        event.name = "CDVD Read";
        event.udata = cdvd;
        event.callback = do_read;
        event.cycles = 1000;

        scheduler::schedule(cdvd->hw.sched, event);

        set_status(cdvd, STATUS_READING);

        return;
    }

    cdvd->n_stat = 0x4e;
    cdvd->n_cmd = 0;

    set_ready(cdvd);
    set_status(cdvd, STATUS_PAUSED);

    send_irq(cdvd);

    // I_STAT needs to be set to 3?
    cdvd->i_stat |= 2;
}

static inline void n_nop(Cdvd* cdvd) {
    iris_debug(cdvd, "nop");

    set_ready(cdvd);
    send_irq(cdvd);
}
static inline void n_nop_sync(Cdvd* cdvd) {
    iris_fatal_error(cdvd, "nop_sync");
}
static inline void n_standby(Cdvd* cdvd) {
    iris_debug(cdvd, "standby");

    set_ready(cdvd);
    set_status(cdvd, STATUS_PAUSED);

    send_irq(cdvd);
}
static inline void n_stop(Cdvd* cdvd) {
    iris_debug(cdvd, "stop");

    set_ready(cdvd);
    set_status(cdvd, STATUS_STOPPED);

    send_irq(cdvd);
}
static inline void n_pause(Cdvd* cdvd) {
    iris_debug(cdvd, "pause");

    set_ready(cdvd);
    set_status(cdvd, STATUS_PAUSED);

    send_irq(cdvd);

    cdvd->n_cmd = 0;
}
static inline void n_seek(Cdvd* cdvd) {
    iris_debug(cdvd, "seek");

    cdvd->read_lba = *(uint32_t*)(cdvd->n_params);

    set_ready(cdvd);
    set_status(cdvd, STATUS_SEEKING);

    send_irq(cdvd);
}
static inline void n_read_cd(Cdvd* cdvd) {
    /*  Params:
        0-3   Sector position
        4-7   Sectors to read
        10    Block size (1=2328 bytes, 2=2340 bytes, all others=2048 bytes)

            Performs a CD-style read. Seems to raise bit 0 of CDVD I_STAT upon completion?
    */

    int prev_lba = cdvd->read_lba;

    cdvd->read_lba = *(uint32_t*)(cdvd->n_params);
    cdvd->read_count = *(uint32_t*)(cdvd->n_params + 4);
    cdvd->read_speed = cdvd->n_params[9];
    cdvd->read_size = CD_SS_2048;

    switch (cdvd->n_params[10]) {
        case 1: cdvd->read_size = CD_SS_2328; break;
        case 2: cdvd->read_size = CD_SS_2340; break;
    }

    scheduler::Event event;

    event.name = "CDVD ReadCd";
    event.udata = cdvd;
    event.callback = do_read;
    event.cycles = get_read_timing(cdvd, 0, prev_lba);

    scheduler::schedule(cdvd->hw.sched, event);

    set_status(cdvd, STATUS_READING);

    // iris_debug(cdvd, "ReadCd lba={:08x} count={:08x} size={} cycles={} speed={:02x} ({})", //     cdvd->read_lba,
    //     cdvd->read_count,
    //     cdvd->read_size,
    //     event.cycles,
    //     cdvd->n_params[9],
    //     cdvd->disc->read_sector
    //);
}
static inline void n_read_cdda(Cdvd* cdvd) {
    int prev_lba = cdvd->read_lba;

    cdvd->read_lba = *(uint32_t*)(cdvd->n_params);
    cdvd->read_count = *(uint32_t*)(cdvd->n_params + 4);
    cdvd->read_speed = cdvd->n_params[9];
    cdvd->read_size = CD_SS_2352;

    // iris_debug(cdvd, "ReadCdda lba={} count={} decode={}", cdvd->read_lba, cdvd->read_count, cdvd->mecha_decode);

    scheduler::Event event;

    event.name = "CDVD ReadCdda";
    event.udata = cdvd;
    event.callback = do_read;
    event.cycles = ((2352.f / 2.f) / 44100.f) * (36864000.f * 8);

    set_status(cdvd, STATUS_READING);

    scheduler::schedule(cdvd->hw.sched, event);
}
static inline void n_read_dvd(Cdvd* cdvd) {
    /*  Params:
        0-3   Sector position
        4-7   Sectors to read

        Performs a DVD-style read, with a block size of 2064 bytes. The format of the data is as follows:
        0    1    Volume number + 0x20
        1    3    Sector number - volume start + 0x30000, in big-endian.
        4    8    ? (all zeroes)
        12   2048 Raw sector data
        2060 4    ? (all zeroes)
    */

    int prev_lba = cdvd->read_lba;

    cdvd->read_lba = *(uint32_t*)(cdvd->n_params);
    cdvd->read_count = *(uint32_t*)(cdvd->n_params + 4);
    cdvd->read_speed = cdvd->n_params[9];
    cdvd->read_size = DVD_SS;

    scheduler::Event event;

    event.name = "CDVD ReadDvd";
    event.udata = cdvd;
    event.callback = do_read;
    event.cycles = get_read_timing(cdvd, 1, prev_lba) >> 3;

    scheduler::schedule(cdvd->hw.sched, event);

    set_status(cdvd, STATUS_READING);
}
static inline void n_get_toc(Cdvd* cdvd) {
    iris_debug(cdvd, "get_toc");

    memset(cdvd->buf, 0, 2064);

    if (cdvd->disc_type == iop::disc::CDVD_DISC_CDDA) {
        int track_count = iop::disc::get_track_count(cdvd->disc);
        int disc_size = iop::disc::get_size(cdvd->disc) / 2352;

        int size_mm = disc_size / (60 * 75);
        int size_ss = (disc_size % (60 * 75)) / 75;
        int size_ff = (disc_size % (60 * 75)) % 75;

        cdvd->buf[0] = 0x41;
        cdvd->buf[1] = 0x00;

        //Number of FirstTrack
        cdvd->buf[2] = 0xa0;
        cdvd->buf[7] = 0x01;

        //Number of LastTrack
        cdvd->buf[12] = 0xa1;
        cdvd->buf[17] = itob_table[track_count];

        //DiskLength
        cdvd->buf[22] = 0xa2;
        cdvd->buf[27] = itob_table[size_mm]; // mm
        cdvd->buf[28] = itob_table[size_ss]; // ss
        cdvd->buf[29] = itob_table[size_ff]; // ff

        for (int i = 0; i < track_count; i++) {
            int num = i + 1;

            iop::disc::TrackInfo info;

            iop::disc::get_track_info(cdvd->disc, num, &info);

            int track_mm = info.lba / (60 * 75);
            int track_ss = (info.lba % (60 * 75)) / 75;
            int track_ff = (info.lba % (60 * 75)) % 75;

            // iris_debug(cdvd, "track {} {}:{}:{}", i + 1, track_mm, track_ss, track_ff);

            cdvd->buf[30 + (i * 10)] = info.type;
            cdvd->buf[32 + (i * 10)] = itob_table[num]; // Track number
            cdvd->buf[37 + (i * 10)] = itob_table[track_mm]; // mm
            cdvd->buf[38 + (i * 10)] = itob_table[track_ss]; // ss
            cdvd->buf[39 + (i * 10)] = itob_table[track_ff]; // ff
        }
    } else if (!is_dual_layer(cdvd)) {
        cdvd->buf[0] = 0x04;
        cdvd->buf[1] = 0x02;
        cdvd->buf[2] = 0xF2;
        cdvd->buf[3] = 0x00;
        cdvd->buf[4] = 0x86;
        cdvd->buf[5] = 0x72;
        cdvd->buf[17] = 0x03;
    } else {
        cdvd->buf[0] = 0x24;
        cdvd->buf[1] = 0x02;
        cdvd->buf[2] = 0xF2;
        cdvd->buf[3] = 0x00;
        cdvd->buf[4] = 0x41;
        cdvd->buf[5] = 0x95;

        cdvd->buf[14] = 0x60;

        cdvd->buf[16] = 0x00;
        cdvd->buf[17] = 0x03;
        cdvd->buf[18] = 0x00;
        cdvd->buf[19] = 0x00;

        int32_t start = cdvd->layer2_lba + 0x30000 - 1;

        cdvd->buf[20] = start >> 24;
        cdvd->buf[21] = (start >> 16) & 0xff;
        cdvd->buf[22] = (start >> 8) & 0xff;
        cdvd->buf[23] = start & 0xFF;
    }

    cdvd->buf_size = 2064;
    cdvd->n_stat = 0x40;

    iop::dma::handle_cdvd_transfer(cdvd->hw.dma);

    set_ready(cdvd);
    set_status(cdvd, STATUS_READING);

    send_irq(cdvd);

    cdvd->n_cmd = 0;
}
static inline void n_read_key(Cdvd* cdvd) {
    uint32_t b0 = cdvd->n_params[3];
    uint32_t b1 = cdvd->n_params[4];
    uint32_t b2 = cdvd->n_params[5];
    uint32_t b3 = cdvd->n_params[6];
    uint32_t arg = b0 | (b1 << 8) | (b2 << 16) | (b3 << 24);

    // Code referenced/taken from PCSX2
    // This performs some kind of encryption/checksum with the game's serial?
    memset(cdvd->cdkey, 0, 16);

    char serial[16];

    if (!iop::disc::get_serial(cdvd->disc, serial)) {
        iris_debug(cdvd, "Couldn't find game serial, can't get cdkey");
    } else {
        iris_debug(cdvd, "\'{}\'", serial);
    }

    int32_t letters = (int32_t)((serial[3] & 0x7F) << 0) |
                (int32_t)((serial[2] & 0x7F) << 7) |
                (int32_t)((serial[1] & 0x7F) << 14) |
                (int32_t)((serial[0] & 0x7F) << 21);

    char m[6];

    for (int i = 0; i < 3; i++)
        m[i] = serial[i+5];

    for (int i = 0; i < 2; i++)
        m[i+3] = serial[i+9];

    m[5] = '\0';
    
    int32_t code = strtoul(m, NULL, 10);

    uint32_t key_0_3 = ((code & 0x1FC00) >> 10) | ((0x01FFFFFF & letters) << 7);
    uint32_t key_4 = ((code & 0x0001F) << 3) | ((0x0E000000 & letters) >> 25);
    uint32_t key_14 = ((code & 0x003E0) >> 2) | 0x04;

    cdvd->cdkey[0] = (key_0_3 & 0x000000FF) >> 0;
    cdvd->cdkey[1] = (key_0_3 & 0x0000FF00) >> 8;
    cdvd->cdkey[2] = (key_0_3 & 0x00FF0000) >> 16;
    cdvd->cdkey[3] = (key_0_3 & 0xFF000000) >> 24;
    cdvd->cdkey[4] = key_4;

    switch (arg) {
        case 75: {
            cdvd->cdkey[14] = key_14;
            cdvd->cdkey[15] = 0x05;
        } break;
        case 4246: {
            cdvd->cdkey[0] = 0x07;
            cdvd->cdkey[1] = 0xF7;
            cdvd->cdkey[2] = 0xF2;
            cdvd->cdkey[3] = 0x01;
            cdvd->cdkey[4] = 0x00;
            cdvd->cdkey[15] = 0x01;
        } break;
        default: {
            cdvd->cdkey[15] = 0x01;
        } break;
    }

    set_ready(cdvd);
    send_irq(cdvd);
}

static inline void n_chg_spdl_ctrl(Cdvd* cdvd) {
    set_ready(cdvd);
    send_irq(cdvd);
}

static inline void handle_n_command(Cdvd* cdvd, uint8_t cmd) {
    cdvd->n_cmd = cmd;

    // iris_debug(cdvd, "N command {} ({:02x})", get_n_command_name(cmd), cmd);

    set_busy(cdvd);

    switch (cdvd->n_cmd) {
        case 0x00: n_nop(cdvd); break;
        case 0x01: n_nop_sync(cdvd); break;
        case 0x02: n_standby(cdvd); break;
        case 0x03: n_stop(cdvd); break;
        case 0x04: n_pause(cdvd); break;
        case 0x05: n_seek(cdvd); break;
        case 0x06: n_read_cd(cdvd); break;
        case 0x07: n_read_cdda(cdvd); break;
        case 0x08: n_read_dvd(cdvd); break;
        case 0x09: n_get_toc(cdvd); break;
        case 0x0c: n_read_key(cdvd); break;
        case 0x0f: n_chg_spdl_ctrl(cdvd); break;
        default: {
            iris_fatal_error(cdvd, "Unhandled N command {:02x}", cdvd->n_cmd);
        } break;
    }

    // Reset N param FIFO
    cdvd->n_param_index = 0;
}

static inline void handle_n_param(Cdvd* cdvd, uint8_t param) {
    cdvd->n_params[cdvd->n_param_index++] = param;

    if (cdvd->n_param_index > 15) {
        iris_fatal_error(cdvd, "N parameter FIFO overflow");
    }
}

Cdvd* create(logger::Logger* logger, iop::intc::Intc* intc, scheduler::Scheduler* sched) {
    Cdvd* cdvd = new Cdvd();

    cdvd->logger = logger;
    cdvd->logger_id = logger::register_source(logger, "cdvd");

#ifdef IRIS_ENABLE_MAGICGATE
    mg::init(&cdvd->mecha_keys, logger);
#endif

    cdvd->hw.intc = intc;
    cdvd->hw.sched = sched;

    // 00:02:00
    cdvd->read_lba = 0x150;

    cdvd->n_stat = 0x4e;
    cdvd->s_stat = S_STATUS_NO_DATA;
    cdvd->sticky_status = 0x1e;

    return cdvd;
}

void connect(Cdvd* cdvd, iop::dma::Dma* dma) {
    cdvd->hw.dma = dma;
}

void destroy(Cdvd* cdvd) {
    close(cdvd);

    free(cdvd->s_fifo);
    delete cdvd;
}

void set_detected_type(void* udata, int overshoot) {
    Cdvd* cdvd = (Cdvd*)udata;

    set_status(cdvd, STATUS_PAUSED);

    cdvd->disc_type = cdvd->detected_disc_type;
}

int open(Cdvd* cdvd, const char* path, int delay) {
    close(cdvd);

    cdvd->layer2_lba = 0;

    cdvd->disc = iop::disc::open(cdvd->logger, path);

    if (!cdvd->disc) {
        iris_debug(cdvd, "Couldn't open disc \'{}\'", path);

        return 1;
    }

    cdvd->detected_disc_type = iop::disc::get_type(cdvd->disc);
    cdvd->layer2_lba = iop::disc::get_volume_lba(cdvd->disc, 1);

    iris_debug(cdvd, "Opened \'{}\' ({})", path, get_type_name(cdvd->detected_disc_type));

    if (!delay) {
        set_status(cdvd, STATUS_SPINNING);

        cdvd->disc_type = cdvd->detected_disc_type;

        return 0;
    }

    switch (cdvd->detected_disc_type) {
        case iop::disc::CDVD_DISC_PS2_CD:
        case iop::disc::CDVD_DISC_PS2_CDDA:
        case iop::disc::CDVD_DISC_CDDA:
        case iop::disc::CDVD_DISC_PSX_CD:
        case iop::disc::CDVD_DISC_PSX_CDDA: {
            cdvd->disc_type = iop::disc::CDVD_DISC_DETECTING_CD;
        } break;

        case iop::disc::CDVD_DISC_PS2_DVD:
        case iop::disc::CDVD_DISC_DVD_VIDEO: {
            cdvd->disc_type = cdvd->layer2_lba ? iop::disc::CDVD_DISC_DETECTING_DL_DVD : iop::disc::CDVD_DISC_DETECTING_DVD;
        } break;
    }

    set_status(cdvd, STATUS_TRAY_OPEN_BIT);

    scheduler::Event event;

    event.cycles = delay; // IOP clock * 2 = 2s
    event.udata = cdvd;
    event.name = "CDVD disc detect";
    event.callback = set_detected_type;

    scheduler::schedule(cdvd->hw.sched, event);

    return 0;
}

void close(Cdvd* cdvd) {
    if (cdvd->disc) {
        iop::disc::close(cdvd->disc);

        cdvd->disc = NULL;
    }

    cdvd->disc_type = iop::disc::CDVD_DISC_NO_DISC;

    set_status(cdvd, STATUS_TRAY_OPEN_BIT);

    // Send disc ejected IRQ
    cdvd->i_stat = IRQ_DISC_EJECTED;

    iop::intc::irq(cdvd->hw.intc, iop::intc::CDVD);
}

void power_off(Cdvd* cdvd) {
    // Send poweroff IRQ
    cdvd->i_stat = IRQ_POWER_OFF;

    iop::intc::irq(cdvd->hw.intc, iop::intc::CDVD);
}

uint8_t read_speed(Cdvd* cdvd) {
    uint8_t speed = cdvd->read_speed & 0x3F;

    if (!speed)
        speed = (cdvd->disc_type == iop::disc::CDVD_DISC_PS2_DVD) ? 3 : 5;

    if (cdvd->disc_type == iop::disc::CDVD_DISC_PS2_DVD)
        speed += 0xF;
    else
        speed--;

    return speed;
}

uint64_t read8(Cdvd* cdvd, uint32_t addr) {
    // iris_debug(cdvd, "read {:08x}", addr);

    switch (addr) {
        case 0x1F402004: iris_debug(cdvd, "read n_cmd {:x}", cdvd->n_cmd); return cdvd->n_cmd;
        case 0x1F402005: iris_debug(cdvd, "read n_stat {:x}", cdvd->n_stat); return cdvd->n_stat;
        // case 0x1F402005: (W)
        case 0x1F402006: iris_debug(cdvd, "read error {:x}", 0); return 0; //cdvd->error;
        // case 0x1F402007: (W)
        case 0x1F402008: iris_debug(cdvd, "read i_stat {:x}", cdvd->i_stat); return cdvd->i_stat;
        case 0x1F40200A: iris_debug(cdvd, "read status {:x}", cdvd->status); return cdvd->status;
        case 0x1F40200B: iris_debug(cdvd, "read sticky_status {:x}", cdvd->sticky_status); return cdvd->sticky_status;
        case 0x1F40200F: iris_debug(cdvd, "read disc_type {:x}", cdvd->disc_type); return cdvd->disc_type;
        case 0x1F402013: iris_debug(cdvd, "read speed {:x}", read_speed(cdvd)); return read_speed(cdvd);
        case 0x1F402015: return 0xff;
        case 0x1F402016: iris_debug(cdvd, "read s_cmd {:x}", cdvd->s_cmd); return cdvd->s_cmd;
        case 0x1F402017: iris_debug(cdvd, "read s_stat {:x}", cdvd->s_stat); return cdvd->s_stat;
        // case 0x1F402017: (W);
        case 0x1F402018: { int r = read_s_response(cdvd); iris_debug(cdvd, "read s_response {:x}", r); return r; }

        case 0x1F402020:
        case 0x1F402021:
        case 0x1F402022:
        case 0x1F402023:
        case 0x1F402024:
            iris_debug(cdvd, "ReadKey {:08x} ({}) -> {:02x}", addr, addr - 0x1f402020, cdvd->cdkey[addr - 0x1F402020]);
            return cdvd->cdkey[addr - 0x1F402020];
        case 0x1F402028:
        case 0x1F402029:
        case 0x1F40202A:
        case 0x1F40202B:
        case 0x1F40202C:
            iris_debug(cdvd, "ReadKey {:08x} ({}) -> {:02x}", addr, addr - 0x1f402023, cdvd->cdkey[addr - 0x1F402023]);
            return cdvd->cdkey[addr - 0x1F402023];
        case 0x1F402030:
        case 0x1F402031:
        case 0x1F402032:
        case 0x1F402033:
        case 0x1F402034:
            iris_debug(cdvd, "ReadKey {:08x} ({}) -> {:02x}", addr, addr - 0x1f402026, cdvd->cdkey[addr - 0x1F402026]);
            return cdvd->cdkey[addr - 0x1F402026];
        case 0x1F402038:
            iris_debug(cdvd, "ReadKey {:08x} ({}) -> {:02x}", addr, addr - 0x1f402038, cdvd->cdkey[15]);
            return cdvd->cdkey[15];
    }

    iris_debug(cdvd, "unknown read {:08x}", addr);
    
    return 0;
}

void write8(Cdvd* cdvd, uint32_t addr, uint64_t data) {
    // iris_debug(cdvd, "write {:08x} {:02x}", addr, data);

    switch (addr) {
        case 0x1F402004: handle_n_command(cdvd, data); return;
        case 0x1F402005: handle_n_param(cdvd, data); return;
        case 0x1F402006: /* Read-only */ return;
        case 0x1F402007: iris_debug(cdvd, "break"); /* To-do: BREAK */ return;
        case 0x1F402008: cdvd->i_stat &= ~data; return;
        case 0x1F40200A: return;
        case 0x1F40200B: return;
        case 0x1F40200F: return;
        case 0x1F402016: handle_s_command(cdvd, data); return;
        case 0x1F402017: handle_s_param(cdvd, data); return;
        // case 0x1F402017: (W);
        case 0x1F40203A: cdvd->mecha_decode = data; return;
        case 0x1F402018: return;
    }

    return;
}

void reset(Cdvd* cdvd) {
    cdvd->n_stat = 0x4c;
    cdvd->read_lba = 0x150;
    cdvd->read_count = 0;
    cdvd->read_size = 0;
    cdvd->read_speed = 0;

    cdvd->config_rw = 0;
    cdvd->config_offset = 0;
    cdvd->config_numblocks = 0;
    cdvd->config_block_index = 0;
    cdvd->s_cmd = 0;
    cdvd->buf_size = 0;
}

#ifdef IRIS_ENABLE_MAGICGATE
int load_mg_key(Cdvd* cdvd, int which, const char* path) {
    return mg::load_file(&cdvd->mecha_keys, which, path);
}

int derive_mg_keys(Cdvd* cdvd, int mode) {
    cdvd->mecha_keys.ready = 0;

    if (!mg::has_complete_keyset(&cdvd->mecha_keys)) {
        bool loaded = false;

        for (int i = 0; i < mg::KEY_FILE_COUNT; i++) {
            if (cdvd->mecha_keys.loaded[i]) {
                loaded = true;

                break;
            }
        }

        if (loaded) {
            iris_warning(cdvd, "MagicGate keyset incomplete, falling back to HLE");
        }

        return 0;
    }

    if (!cdvd->mecha_state) {
        cdvd->mecha_state = new mg::State();
    }

    mg::reset(cdvd->mecha_state);

    const uint8_t* console_id = &cdvd->nvram[cdvd->layout.console_id_offset];
    const uint8_t* ilink_id = &cdvd->nvram[cdvd->layout.ilink_id_offset];

    if (!mg::derive(&cdvd->mecha_keys, mode, console_id, ilink_id)) {
        iris_error(cdvd, "MagicGate: derive failed (mode {})", mode);

        return 0;
    }

    iris_info(cdvd, "MagicGate key store ready (mode {})", mode);

    return 1;
}

int mg_ready(Cdvd* cdvd) {
    return cdvd->mecha_keys.ready;
}

const uint8_t* mg_challenge_iv(Cdvd* cdvd) {
    return cdvd->mecha_keys.store.challenge_iv;
}
#else
int load_mg_key(Cdvd* cdvd, int which, const char* path) {
    return 0;
}

int derive_mg_keys(Cdvd* cdvd, int mode) {
    return 0;
}

int mg_ready(Cdvd* cdvd) {
    return 0;
}

const uint8_t* mg_challenge_iv(Cdvd* cdvd) {
    return nullptr;
}
#endif

void set_mg_enabled(Cdvd* cdvd, int enabled) {
    cdvd->mg_enabled = enabled;
}

void set_mechacon_model(Cdvd* cdvd, int model) {
    cdvd->mechacon_model = model;

    switch (model) {
        case MECHACON_SPC970: {
            cdvd->layout = g_spc970_layout;
        } break;

        case MECHACON_DRAGON: {
            cdvd->layout = g_dragon_layout;
        } break;
    }
}

int load_nvram(Cdvd* cdvd, const char* path) {
    FILE* file = fopen(path, "rb");

    if (!file) {
        memset(cdvd->nvram, 0, sizeof(cdvd->nvram));
    } else {
        fread(cdvd->nvram, 1, 1024, file);
        fclose(file);
    }

    strncpy(cdvd->nvram_path, path, sizeof(cdvd->nvram_path) - 1);

    cdvd->nvram_path[sizeof(cdvd->nvram_path) - 1] = 0;

    return file != nullptr;
}

static void nvram_writeback(Cdvd* cdvd) {
    if (!cdvd->nvram_path[0])
        return;

    FILE* file = fopen(cdvd->nvram_path, "r+b");

    if (!file)
        file = fopen(cdvd->nvram_path, "wb");

    if (!file) {
        iris_debug(cdvd, "NVRAM writeback failed to open '{}'", cdvd->nvram_path);

        return;
    }

    fwrite(cdvd->nvram, 1, 1024, file);
    fclose(file);
}

}
