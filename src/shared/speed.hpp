#pragma once

#include "scheduler.hpp"
#include "iop/intc.hpp"
#include "speed/ata.hpp"
#include "speed/dvrp.hpp"
#include "speed/flash.hpp"
#include "speed/eeprom.hpp"
#include "speed/smap.hpp"
#include "logger.hpp"

namespace iris::speed {

// SPEED is a chip presented as a register interface to speed devices.
// This includes:
// - SMAP (Ethernet?) at 0x10000100
// - ATA (HDD) at 0x10000040
// - UART (Maxim MAX232 UART, used in arcade units?) at ?
// - DVR (PSX DESR Digital Video Recorder)
// - Flash (also known as EROM, used in PSX DESR units) at 0x10004800
// It is mapped to phys 0x10000000 on the IOP side, and phys 0x14000000
// on the EE side.
// Software won't even try to access SPEED if it isn't detected through
// the speed_REV (1f80146e) register first.

// Note: SMAP also provides access to the unit's MAC address (used by the BIOS)

// Notes: USB and i.Link (FireWire) are completely separate
//        and not connected to the SPEED interface at all.
// 
//        USB is presented through an OHCI interface at 0x1F801600
//        i.Link is presented through a FireWire interface at 0x1F808400

inline constexpr auto INTR_ATA0 = 0x0001;
inline constexpr auto INTR_ATA1 = 0x0002;
inline constexpr auto INTR_DVR = 0x0200;// mask=0x0200
inline constexpr auto INTR_UART = 0x1000;// mask=0x1000
inline constexpr auto INTR_ATA = (INTR_ATA0 | INTR_ATA1);// mask=0x0003

inline constexpr auto CAPS_SMAP = (1 << 0);
inline constexpr auto CAPS_ATA = (1 << 1);
inline constexpr auto CAPS_UART = (1 << 3);
inline constexpr auto CAPS_DVR = (1 << 4);
inline constexpr auto CAPS_FLASH = (1 << 5);

struct Speed {
    uint16_t rev; // 10000000
    uint16_t rev1; // 10000002
    uint16_t rev3; // 10000004

    int flash_loaded = 0;
    uint16_t rev8; // 1000000e
    uint32_t dma_ctrl; // 10000024
    uint16_t intr_stat; // 10000028
    uint16_t intr_mask; // 1000002a
    uint16_t pio_dir; // 1000002c
    uint16_t pio_data; // 1000002e
    uint32_t xfr_ctrl; // 10000032
    uint32_t unknown38; // 10000038
    uint32_t if_ctrl; // 10000064
    uint32_t pio_mode; // 10000070
    uint32_t mwdma_mode; // 10000072
    uint32_t udma_mode; // 10000074

    ata::Ata* ata;
    flash::Flash* flash;
    eeprom::Eeprom* eeprom;
    dvrp::Dvrp* dvrp;
    smap::Smap* smap;

    iop::intc::Intc* iop_intc;
    scheduler::Scheduler* sched;

    logger::Logger* logger = nullptr;
    size_t logger_id = 0;
};

Speed* create(logger::Logger* logger, iop::intc::Intc* iop_intc, scheduler::Scheduler* sched);
void destroy(Speed* speed);
uint64_t read8(Speed* speed, uint32_t addr);
uint64_t read16(Speed* speed, uint32_t addr);
uint64_t read32(Speed* speed, uint32_t addr);
void write8(Speed* speed, uint32_t addr, uint64_t data);
void write16(Speed* speed, uint32_t addr, uint64_t data);
void write32(Speed* speed, uint32_t addr, uint64_t data);
void send_irq(Speed* speed, uint16_t irq);
int load_hdd(Speed* speed, const char* path);
int load_flash(Speed* speed, const char* path);
void set_dvrp_enabled(Speed* speed, int enabled);
void set_flash_enabled(Speed* speed, int enabled);
void set_smap_enabled(Speed* speed, int enabled);
void set_mac_address(Speed* speed, const uint8_t* mac);

}
