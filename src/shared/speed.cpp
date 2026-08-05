#include <new>

#include "speed.hpp"

namespace iris::speed {

Speed* create(logger::Logger* logger, iop::intc::Intc* iop_intc, scheduler::Scheduler* sched) {
    Speed* speed = new Speed();

    speed->logger = logger;
    speed->logger_id = logger::register_source(logger, "speed");

    speed->iop_intc = iop_intc;
    speed->sched = sched;
    speed->flash = flash::create(speed->logger);
    speed->ata = ata::create(speed->logger);
    speed->eeprom = eeprom::create(speed->logger);
    speed->dvrp = dvrp::create(speed->logger);
    speed->smap = smap::create(speed->logger);

    flash::init(speed->flash);
    ata::init(speed->ata, speed, sched);
    eeprom::init(speed->eeprom);
    dvrp::init(speed->dvrp, speed);
    smap::init(speed->smap, speed);

    // 0009 - TS
    // 0010 - ES1?
    // 0011 - ES2
    speed->rev1 = 0x0011;
    speed->rev8 |= 2;

    return speed;
}

void destroy(Speed* speed) {
    flash::destroy(speed->flash);
    ata::destroy(speed->ata);
    eeprom::destroy(speed->eeprom);
    dvrp::destroy(speed->dvrp);
    smap::destroy(speed->smap);

    delete speed;
}

uint64_t read8(Speed* speed, uint32_t addr) {
    addr &= 0xffff;

    if (addr >= 0x0100 && addr < 0x4000) {
        return smap::read8(speed->smap, addr);
    }

    // iris_debug(speed, "read8 {:08x} {:08x}", addr);

    switch (addr) {
        case 0x002e: return eeprom::read(speed->eeprom);
    }

    return 0;

    // exit(1);
}
uint64_t read16(Speed* speed, uint32_t addr) {
    addr &= 0xffff;

    if (addr >= 0x4800 && addr < 0x4820) {
        return flash::read16(speed->flash, addr);
    }

    if (addr >= 0x0040 && addr < 0x0060) {
        return ata::read16(speed->ata, addr);
    }

    if (addr >= 0x4200 && addr < 0x4240) {
        return dvrp::read(speed->dvrp, addr);
    }

    if (addr >= 0x0100 && addr < 0x4000) {
        return smap::read16(speed->smap, addr);
    }

    switch (addr) {
        case 0x0000: return speed->rev;
        case 0x0002: return speed->rev1;
        case 0x0004: return speed->rev3;
        case 0x000e: return speed->rev8;
        case 0x0024: return speed->dma_ctrl;
        case 0x0028: return speed->intr_stat;
        case 0x002a: return speed->intr_mask;
        case 0x0032: return speed->xfr_ctrl;
        case 0x0038: return speed->unknown38;
        case 0x0064: return speed->if_ctrl;
    }

    iris_debug(speed, "read16 {:08x} <-------------------------------------------", addr); // exit(1);

    return 0;
}
uint64_t read32(Speed* speed, uint32_t addr) {
    addr &= 0xffff;

    if (addr >= 0x0100 && addr < 0x4000) {
        return smap::read32(speed->smap, addr);
    }

    if (addr >= 0x4800 && addr < 0x4820) {
        return flash::read32(speed->flash, addr);
    }

    if (addr >= 0x4200 && addr < 0x4240) {
        return dvrp::read(speed->dvrp, addr);
    }

    // iris_debug(speed, "read32 {:08x}", addr); // exit(1);

    return 0;
}
void write8(Speed* speed, uint32_t addr, uint64_t data) {
    addr &= 0xffff;

    if (addr >= 0x0100 && addr < 0x4000) {
        smap::write8(speed->smap, addr, data);

        return;
    }

    // iris_debug(speed, "write8 {:08x} {:08x}", addr, data);

    switch (addr) {
        case 0x002c: speed->pio_dir = data; return;
        case 0x002e: eeprom::write(speed->eeprom, data); return;
    }

    // exit(1);
}
void write16(Speed* speed, uint32_t addr, uint64_t data) {
    addr &= 0xffff;

    if (addr >= 0x0100 && addr < 0x4000) {
        smap::write16(speed->smap, addr, data);

        return;
    }

    if (addr >= 0x4800 && addr < 0x4820) {
        flash::write16(speed->flash, addr, data);

        return;
    }

    if (addr >= 0x0040 && addr < 0x0060) {
        ata::write16(speed->ata, addr, data);

        return;
    }

    if (addr >= 0x4200 && addr < 0x4240) {
        dvrp::write(speed->dvrp, addr, data);

        return;
    }

    switch (addr) {
        case 0x0024: speed->dma_ctrl = data; return;
        case 0x0032: speed->xfr_ctrl = data; return;
        case 0x0038: speed->unknown38 = data; /* ??? */ return;
        case 0x0064: speed->if_ctrl = data; return;
        case 0x0070: speed->pio_mode = data; return;
        case 0x0072: speed->mwdma_mode = data; return;
        case 0x0074: speed->udma_mode = data; return;
        case 0x002a: speed->intr_mask = data; return;
    }

    // iris_debug(speed, "write16 {:08x} {:04x} <-------------------------------------------", addr, (uint16_t)data); // exit(1);

    // iris_debug(speed, "write16 {:08x} {:08x}", addr, data); // exit(1);
}
void write32(Speed* speed, uint32_t addr, uint64_t data) {
    addr &= 0xffff;

    if (addr >= 0x0100 && addr < 0x4000) {
        smap::write32(speed->smap, addr, data);

        return;
    }

    if (addr >= 0x4800 && addr < 0x4820) {
        flash::write32(speed->flash, addr, data);

        return;
    }

    if (addr >= 0x4200 && addr < 0x4240) {
        dvrp::write(speed->dvrp, addr, data);

        return;
    }

    // iris_debug(speed, "write32 {:08x} {:08x}", addr, data); // exit(1);
}

void send_irq(Speed* speed, uint16_t irq) {
    speed->intr_stat |= irq;

    if (speed->intr_stat & speed->intr_mask) {
        iop::intc::irq(speed->iop_intc, iop::intc::DEV9);
    }
}

int load_flash(Speed* speed, const char* path) {
    int ret = flash::load(speed->flash, path);

    if (ret) {
        speed->rev3 |= CAPS_FLASH;
    }

    return ret;
}

int load_hdd(Speed* speed, const char* path) {
    int ret = ata::load(speed->ata, path);

    if (ret) {
        speed->rev3 |= CAPS_ATA;
    }

    return ret;
}

void set_mac_address(Speed* speed, const uint8_t* mac) {
    uint16_t data[32] = {
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x1000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0010, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000,
        0x1000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000
    };

    data[0] = (mac[1] << 8) | mac[0];
    data[1] = (mac[3] << 8) | mac[2];
    data[2] = (mac[5] << 8) | mac[4];
    data[3] = data[0] + data[1] + data[2];

    eeprom::load(speed->eeprom, data);
}

void set_dvrp_enabled(Speed* speed, int enabled) {
    if (enabled) {
        speed->rev3 |= CAPS_DVR;
    } else {
        speed->rev3 &= ~CAPS_DVR;
    }
}

void set_smap_enabled(Speed* speed, int enabled) {
    iris_debug(speed, "set smap enabled {}", enabled);

    if (enabled) {
        speed->rev3 |= CAPS_SMAP;
    } else {
        speed->rev3 &= ~CAPS_SMAP;
    }
}

}
