#pragma once
#include "logger.hpp"

namespace iris::iop::disc {

#ifdef _MSC_VER
#define fseek64 _fseeki64
#define ftell64 _ftelli64
#elif defined(_WIN32)
#define fseek64 fseeko64
#define ftell64 ftello64
#else
#define fseek64 fseek
#define ftell64 ftell
#endif

inline constexpr auto DISC_ERR_CANT_OPEN = 0;
inline constexpr auto DISC_ERR_ISO_INVALID = 1;
inline constexpr auto DISC_ERR_UNSUPPORTED_SS = 2;

inline constexpr auto DISC_EXT_ISO = 0;
inline constexpr auto DISC_EXT_BIN = 1;
inline constexpr auto DISC_EXT_CUE = 2;
inline constexpr auto DISC_EXT_CHD = 3;
inline constexpr auto DISC_EXT_CSO = 4;
inline constexpr auto DISC_EXT_ZSO = 5;
inline constexpr auto DISC_EXT_NONE = 6;
inline constexpr auto DISC_EXT_UNSUPPORTED = 7;

inline constexpr auto DISC_MEDIA_CD = 0;
inline constexpr auto DISC_MEDIA_DVD = 1;

inline constexpr auto DISC_TYPE_INVALID = 0;
inline constexpr auto DISC_TYPE_CDDA = 1;
inline constexpr auto DISC_TYPE_GAME = 2;
inline constexpr auto DISC_TYPE_UNKNOWN = 3;

/*
    00h  No disc
    01h  Detecting
    02h  Detecting CD
    03h  Detecting DVD
    04h  Detecting dual-layer DVD
    05h  Unknown
    10h  PSX CD
    11h  PSX CDDA
    12h  PS2 CD
    13h  PS2 CDDA
    14h  PS2 DVD
    FDh  CDDA (Music)
    FEh  DVDV (Movie disc)
    FFh  Illegal
*/
inline constexpr auto CDVD_DISC_NO_DISC = 0;
inline constexpr auto CDVD_DISC_DETECTING = 1;
inline constexpr auto CDVD_DISC_DETECTING_CD = 2;
inline constexpr auto CDVD_DISC_DETECTING_DVD = 3;
inline constexpr auto CDVD_DISC_DETECTING_DL_DVD = 4;
inline constexpr auto CDVD_DISC_PSX_CD = 16;
inline constexpr auto CDVD_DISC_PSX_CDDA = 17;
inline constexpr auto CDVD_DISC_PS2_CD = 18;
inline constexpr auto CDVD_DISC_PS2_CDDA = 19;
inline constexpr auto CDVD_DISC_PS2_DVD = 20;
inline constexpr auto CDVD_DISC_CDDA = 253;
inline constexpr auto CDVD_DISC_DVD_VIDEO = 254;
inline constexpr auto CDVD_DISC_INVALID = 255;

inline constexpr auto DISC_SS_DATA = 0;
inline constexpr auto DISC_SS_RAW = 1;

inline constexpr auto CDVD_TRACK_AUDIO = 0x01;
inline constexpr auto CDVD_TRACK_MODE1 = 0x41;
inline constexpr auto CDVD_TRACK_MODE2 = 0x61;

struct TrackInfo {
    int number;
    int type;
    uint32_t lba;
};

struct Disc {
    int (*read_sector)(void* udata, unsigned char* buf, uint64_t lba, int size);
    uint64_t (*get_size)(void* udata);
    int (*get_sector_size)(void* udata);
    int (*get_track_count)(void* udata);
    int (*get_track_info)(void* udata, int track, TrackInfo* info);
    int (*get_track_number)(void* udata, uint64_t lba);

    void* udata;

    uint64_t layer2_lba;
    int ext;
    int pvd_cached, system_cnf_cached, root_cached, boot_path_cached;
    char pvd[2048];
    char root[2048];
    char system_cnf[2048];
    char boot_path[256];

    logger::Logger* logger = nullptr;
    size_t logger_id = 0;
};

Disc* open(const char* path);
int read_sector(Disc* disc, unsigned char* buf, uint64_t lba, int size);
int get_type(Disc* disc);
uint64_t get_size(Disc* disc);
uint64_t get_volume_lba(Disc* disc, int vol);
int get_sector_size(Disc* disc);
int get_track_count(Disc* disc);
int get_track_info(Disc* disc, int track, TrackInfo* info);
int get_track_number(Disc* disc, uint64_t lba);
char* get_serial(Disc* disc, char* buf);
char* get_boot_path(Disc* disc);
char* read_boot_elf(Disc* disc, int size);
void close(Disc* disc);

}
