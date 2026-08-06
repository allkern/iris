#pragma once

#include "scheduler.hpp"
#include "dma.hpp"
#include "disc.hpp"
#include "logger.hpp"

namespace iris::cdvd {

/*
    0     Tray status (1=open)
    1     Spindle spinning (1=spinning)
    2     Read status (1=reading data sectors)
    3     Paused
    4     Seek status (1=seeking)
    5     Error (1=error occurred)
    6-7   Unknown
*/
inline constexpr auto STATUS_TRAY_OPEN_BIT = 1;
inline constexpr auto STATUS_SPINNING_BIT = 2;
inline constexpr auto STATUS_READING_BIT = 4;
inline constexpr auto STATUS_PAUSED_BIT = 8;
inline constexpr auto STATUS_SEEKING_BIT = 16;
inline constexpr auto STATUS_ERROR_BIT = 32;
inline constexpr auto STATUS_STOPPED = 0x00;
inline constexpr auto STATUS_SPINNING = 0x02;
inline constexpr auto STATUS_READING = 0x06;
inline constexpr auto STATUS_PAUSED = 0x0A;
inline constexpr auto STATUS_SEEKING = 0x12;

/*
    0     Error (1=error occurred)
    1     Unknown/unused
    2     DEV9 device connected (1=HDD/network adapter connected)
    3     Unknown/unused
    4     Test mode
    5     Power off ready
    6     Drive status (1=ready)
    7     Busy executing NCMD
*/
inline constexpr auto N_STATUS_ERROR = 1;
inline constexpr auto N_STATUS_DEV9_CONNECTED = 4;
inline constexpr auto N_STATUS_TEST_MODE = 16;
inline constexpr auto N_STATUS_POWER_OFF = 32;
inline constexpr auto N_STATUS_READY = 64;
inline constexpr auto N_STATUS_BUSY = 128;

/*
    0     Data ready?
    1     (N?) Command complete
    2     Power off pressed
    3     Disk ejected
    4     BS_Power DET?
    5-7   Unused
*/
inline constexpr auto IRQ_DATA_READY = 1;
inline constexpr auto IRQ_NCMD_DONE = 2;
inline constexpr auto IRQ_POWER_OFF = 4;
inline constexpr auto IRQ_DISC_EJECTED = 8;
inline constexpr auto IRQ_BS_POWER = 16;

/*
    0-5   Unknown
    6     Result data available (0=available, 1=no data)
    7     Busy
*/
inline constexpr auto S_STATUS_NO_DATA = 64;
inline constexpr auto S_STATUS_BUSY = 128;

inline constexpr auto CD_SS_2328 = 2328;
inline constexpr auto CD_SS_2340 = 2340;
inline constexpr auto CD_SS_2048 = 2048;
inline constexpr auto CD_SS_2352 = 2352;
inline constexpr auto DVD_SS = 2064;

struct NvramLayout {
    uint32_t bios_version;   // bios version that this eeprom layout is for
    int32_t config0_offset;   // offset of 1st config block
    int32_t config1_offset;   // offset of 2nd config block
    int32_t config2_offset;   // offset of 3rd config block
    int32_t console_id_offset; // offset of console id (?)
    int32_t ilink_id_offset;   // offset of ilink id (ilink mac address)
    int32_t modelnum_offset;  // offset of ps2 model number (eg "SCPH-70002")
    int32_t regparams_offset; // offset of RegionParams for PStwo
    int32_t mac_offset;       // offset of MAC address on PStwo
};

enum {
    MECHACON_SPC970,
    MECHACON_DRAGON
};

struct Cdvd {
    struct {
        iop::dma::Dma* dma;
        iop::intc::Intc* intc;
        scheduler::Scheduler* sched;
    } hw;

    uint8_t  mg_buffer[0x1000];
    uint16_t mg_size;
    uint16_t mg_maxsize;
    uint8_t  mg_datatype;
    uint8_t  mg_kbit[16];
    uint8_t  mg_kcon[16];
    uint8_t n_cmd;
    uint8_t n_stat;
    uint8_t error;
    uint8_t i_stat;
    uint8_t status;
    uint8_t sticky_status;
    uint8_t disc_type;
    uint8_t s_cmd;
    uint8_t s_stat;

#ifdef _MSC_VER
    __declspec(align(4)) uint8_t n_params[16];
    __declspec(align(4)) uint8_t s_params[16];
#else
    uint8_t n_params[16] __attribute__((aligned(4)));
    uint8_t s_params[16] __attribute__((aligned(4)));
#endif

    uint8_t* s_fifo;
    int n_param_index;
    int s_param_index;
    int s_fifo_index;
    int s_fifo_size;

    uint8_t detected_disc_type;

    uint8_t mecha_decode;
    uint8_t cdkey[16];

    iop::disc::Disc* disc;
    uint8_t buf[2352];
    int buf_size;

    // Pending read
    uint32_t read_lba;
    uint32_t read_count;
    uint32_t read_size;
    uint8_t read_speed;

    uint8_t nvram[1024];
    char nvram_path[1024];

    uint64_t layer2_lba;

    uint32_t config_rw;
    uint32_t config_offset;
    uint32_t config_numblocks;
    uint32_t config_block_index;

    int mechacon_model;
    NvramLayout layout;

    // To-do:
    // void (*poweroff_handler)(void* udata)
    // void (*trayctrl_handler)(void* udata, uint8_t ctrl)

    logger::Logger* logger = nullptr;
    size_t logger_id = 0;
};

Cdvd* create(logger::Logger* logger, iop::intc::Intc* intc, scheduler::Scheduler* sched);
void connect(Cdvd* cdvd, iop::dma::Dma* dma);
void destroy(Cdvd* cdvd);
int open(Cdvd* cdvd, const char* path, int delay);
void close(Cdvd* cdvd);
void power_off(Cdvd* cdvd);
int load_nvram(Cdvd* cdvd, const char* path);
void set_mechacon_model(Cdvd* cdvd, int model);
uint64_t read8(Cdvd* cdvd, uint32_t addr);
void write8(Cdvd* cdvd, uint32_t addr, uint64_t data);
void reset(Cdvd* cdvd);

#undef ALIGNED_U32

}
