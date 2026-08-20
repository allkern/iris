#include <new>

#include "bus.hpp"
#include "gs/gs.hpp"
#include "iop/cdvd.hpp"
#include "bus_decl.hpp"

namespace iris::ee::bus {

Bus* create(logger::Logger* logger) {
    Bus* bus = new Bus();

    bus->logger = logger;
    bus->logger_id = logger::register_source(logger, "ee_bus");

    return bus;
}

#define likely(x) __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)

inline constexpr auto FASTMEM_LIMIT = 0x20000000;

static inline void* fastmem_read_ptr(Bus* bus, uint32_t addr) {
    if (addr >= FASTMEM_LIMIT)
        return nullptr;

    return bus->fastmem_r_table[addr >> 13];
}

static inline void* fastmem_write_ptr(Bus* bus, uint32_t addr) {
    if (addr >= FASTMEM_LIMIT)
        return nullptr;

    return bus->fastmem_w_table[addr >> 13];
}

void init_fastmem(Bus* bus, int ee_ram_size, int iop_ram_size) {
    memset(bus->fastmem_r_table, 0, sizeof(bus->fastmem_r_table));
    memset(bus->fastmem_w_table, 0, sizeof(bus->fastmem_w_table));

    // BIOS
    for (int i = 0; i < 0x200; i++) {
        bus->fastmem_r_table[i+0xfe00] = bus->bios->buf + (i * 0x2000);
    }

    // Main RAM
    for (int i = 0; i < (ee_ram_size / 0x2000); i++) {
        bus->fastmem_r_table[i+0x0000] = bus->ee_ram->buf + (i * 0x2000);
        bus->fastmem_w_table[i+0x0000] = bus->ee_ram->buf + (i * 0x2000);
    }

    // IOP RAM
    for (int i = 0; i < (iop_ram_size / 0x2000); i++) {
        bus->fastmem_r_table[i+0xe000] = bus->iop_ram->buf + (i * 0x2000);
        // bus->fastmem_w_table[i+0xe000] = bus->iop_ram->buf + (i * 0x2000);
    }
}

void init_kputchar(Bus* bus, void (*kputchar)(void*, char), void* udata) {
    bus->kputchar = kputchar;
    bus->kputchar_udata = udata;
}

void destroy(Bus* bus) {
    delete bus;
}

#define MAP_MEM_READ_NS(b, l, u, d, n)     if ((addr >= l) && (addr <= u)) return d::read ## b (bus->n, addr - l);

#define MAP_REG_READ_NS(b, l, u, d, n)     if ((addr >= l) && (addr <= u)) return d::read ## b (bus->n, addr);

#define MAP_MEM_WRITE_NS(b, l, u, d, n)     if ((addr >= l) && (addr <= u)) { d::write ## b (bus->n, addr - l, data); return; }

#define MAP_REG_WRITE_NS(b, l, u, d, n)     if ((addr >= l) && (addr <= u)) { d::write ## b (bus->n, addr, data); return; }

#define MAP_MEM_READ(b, l, u, d, n) \
    if ((addr >= l) && (addr <= u)) return ps2_ ## d ## _read ## b(bus->n, addr - l);

#define MAP_MEM_WRITE(b, l, u, d, n) \
    if ((addr >= l) && (addr <= u)) { ps2_ ## d ## _write ## b(bus->n, addr - l, data); return; }

#define MAP_REG_READ(b, l, u, d, n) \
    if ((addr >= l) && (addr <= u)) return ps2_ ## d ## _read ## b(bus->n, addr);

#define MAP_REG_WRITE(b, l, u, d, n) \
    if ((addr >= l) && (addr <= u)) { ps2_ ## d ## _write ## b(bus->n, addr, data); return; }

// Fast ranges:
// - RAM   00000000-01FFFFFF -> 0000-0fff (1000)
// - BIOS  1FC00000-1FFFFFFF -> fe00-ffff (200)
// - VU    11000000-1100FFFF -> 8800-8807 (8)
// - IOP   1C000000-1C1FFFFF -> e000-e0ff (100)

uint64_t read8(void* udata, uint32_t addr) {
    Bus* bus = (Bus*)udata;

    void* ptr = fastmem_read_ptr(bus, addr);

    if (likely(ptr)) return *((uint8_t*)(((uint8_t*)ptr) + (addr & 0x1fff)));

    // MAP_MEM_READ_NS(8, 0x00000000, 0x01FFFFFF, ram, ee_ram);
    // MAP_MEM_READ_NS(8, 0x20000000, 0x21FFFFFF, ram, ee_ram);
    // MAP_MEM_READ_NS(8, 0x30000000, 0x31FFFFFF, ram, ee_ram);
    // MAP_MEM_READ_NS(8, 0x1C000000, 0x1C1FFFFF, ram, iop_ram);
    // MAP_MEM_READ_NS(8, 0x1FC00000, 0x1FFFFFFF, bios, bios);
    MAP_MEM_READ_NS(8, 0x11000000, 0x11007FFF, vu, vu0);
    MAP_MEM_READ_NS(8, 0x11008000, 0x1100FFFF, vu, vu1);
    MAP_REG_READ_NS(8, 0x10008000, 0x1000EFFF, dmac, dmac);
    MAP_REG_READ_NS(8, 0x1000F520, 0x1000F5FF, dmac, dmac);
    MAP_REG_READ_NS(8, 0x1F402004, 0x1F402018, cdvd, cdvd);
    MAP_MEM_READ_NS(8, 0x1E000000, 0x1E3FFFFF, bios, rom1);
    MAP_MEM_READ_NS(8, 0x1E400000, 0x1E7FFFFF, bios, rom2);
    MAP_REG_READ_NS(64, 0x12000000, 0x12001FFF, gs, gs); // Reuse 64-bit function
    MAP_REG_READ_NS(8, 0x1F801460, 0x1F80147F, dev9, dev9);
    MAP_REG_READ_NS(8, 0x14000000, 0x1400FFFF, speed, speed);

    if ((addr >> 16) == 0x1f80) return 0;

    // printf("bus: Unhandled 8-bit read from physical address 0x%08x\n", addr); // *(int*)0 = 0;

    return 0;
}

uint64_t read16(void* udata, uint32_t addr) {
    Bus* bus = (Bus*)udata;

    void* ptr = fastmem_read_ptr(bus, addr);

    if (likely(ptr)) return *((uint16_t*)(((uint8_t*)ptr) + (addr & 0x1fff)));

    // MAP_MEM_READ_NS(16, 0x00000000, 0x01FFFFFF, ram, ee_ram);
    // MAP_MEM_READ_NS(16, 0x20000000, 0x21FFFFFF, ram, ee_ram);
    // MAP_MEM_READ_NS(16, 0x30000000, 0x31FFFFFF, ram, ee_ram);
    // MAP_MEM_READ_NS(16, 0x1C000000, 0x1C1FFFFF, ram, iop_ram);
    // MAP_MEM_READ_NS(16, 0x1FC00000, 0x1FFFFFFF, bios, bios);
    MAP_REG_READ_NS(16, 0x10008000, 0x1000EFFF, dmac, dmac);
    MAP_REG_READ_NS(16, 0x1000F520, 0x1000F5FF, dmac, dmac);
    MAP_MEM_READ_NS(16, 0x11000000, 0x11007FFF, vu, vu0);
    MAP_MEM_READ_NS(16, 0x11008000, 0x1100FFFF, vu, vu1);
    MAP_REG_READ_NS(32, 0x10003800, 0x10003BFF, vif, vif0);
    MAP_REG_READ_NS(32, 0x10003C00, 0x10003FFF, vif, vif1);
    MAP_REG_READ_NS(32, 0x10004000, 0x10004FFF, vif, vif0);
    MAP_REG_READ_NS(32, 0x10005000, 0x10005FFF, vif, vif1);
    MAP_MEM_READ_NS(16, 0x1E000000, 0x1E3FFFFF, bios, rom1);
    MAP_MEM_READ_NS(16, 0x1E400000, 0x1E7FFFFF, bios, rom2);
    MAP_REG_READ_NS(16, 0x10000000, 0x10001FFF, timers, timers);
    MAP_REG_READ_NS(16, 0x1F801460, 0x1F80147F, dev9, dev9);
    MAP_REG_READ_NS(16, 0x14000000, 0x1400FFFF, speed, speed);

    if (addr == 0x1a000010) return 0xffff;

    switch (addr) {
        case 0x1f803800: return 0;

        // SCPH-39001 stub
        case 0x1a000006: return 2;
    }

    // fprintf(stderr, "bus: Unhandled 16-bit read from physical address 0x%08x\n", addr); // exit(1);

    return 0;
}

uint64_t read32(void* udata, uint32_t addr) {
    Bus* bus = (Bus*)udata;

    // pacmanbr
    // if (addr == 0x00189A40) return 0x34630009;

    // umilucky
    // if (addr == 0x001A2834) return 0x0;
    // if (addr == 0x001AAC9C) return 0x24040002;
    // if (addr == 0x001AAE08) return 0x0;

    // akaiser
    // if (addr == 0x00102110) return 0x0;
    // if (addr == 0x00102130) return 0x0;
    // if (addr == 0x0013e388) return 0x0;
    // if (addr == 0x001c2b0c) return 0x24027FFF;
    // if (addr == 0x00104d7c) return 0x240F0064;

    // akaievo
    // if (addr == 0x001021E8) return 0x0;
    // if (addr == 0x00102208) return 0x0;
    // if (addr == 0x0015FA18) return 0x0;
    // if (addr == 0x0015FC20) return 0x0;
    // if (addr == 0x001D8A0C) return 0x24027FFF;
    // if (addr == 0x001051b4) return 0x240F02BC;

    // // DoA (J)
    // if (addr == 0x00290408) return 0x24060000;

    // // DoA (E)
    // if (addr == 0x002b4c44) return 0x24060000;

    // DoA (U)
    // if (addr == 0x002b06ec) return 0x24060000;

    void* ptr = fastmem_read_ptr(bus, addr);

    if (likely(ptr)) return *((uint32_t*)(((uint8_t*)ptr) + (addr & 0x1fff)));

    // MAP_MEM_READ_NS(32, 0x00000000, 0x01FFFFFF, ram, ee_ram);
    // MAP_MEM_READ_NS(32, 0x20000000, 0x21FFFFFF, ram, ee_ram);
    // MAP_MEM_READ_NS(32, 0x30000000, 0x31FFFFFF, ram, ee_ram);
    // MAP_MEM_READ_NS(32, 0x1C000000, 0x1C1FFFFF, ram, iop_ram);
    // MAP_MEM_READ_NS(32, 0x1FC00000, 0x1FFFFFFF, bios, bios);
    MAP_REG_READ_NS(32, 0x1000F200, 0x1000F26F, sif, sif);
    MAP_REG_READ_NS(32, 0x10008000, 0x1000EFFF, dmac, dmac);
    MAP_REG_READ_NS(32, 0x1000F520, 0x1000F5FF, dmac, dmac);
    MAP_REG_READ_NS(64, 0x10002000, 0x1000203F, ipu, ipu);
    MAP_REG_READ_NS(64, 0x10007000, 0x1000701F, ipu, ipu);
    MAP_REG_READ_NS(32, 0x10003000, 0x100037FF, gif, gif);
    MAP_REG_READ_NS(32, 0x10003800, 0x10003BFF, vif, vif0);
    MAP_REG_READ_NS(32, 0x10003C00, 0x10003FFF, vif, vif1);
    MAP_REG_READ_NS(32, 0x10004000, 0x10004FFF, vif, vif0);
    MAP_REG_READ_NS(32, 0x10005000, 0x10005FFF, vif, vif1);
    MAP_REG_READ_NS(32, 0x1000F000, 0x1000F01F, intc, intc);
    MAP_REG_READ_NS(64, 0x12000000, 0x12001FFF, gs, gs); // Reuse 64-bit function
    MAP_REG_READ_NS(32, 0x10000000, 0x10001FFF, timers, timers);
    MAP_MEM_READ_NS(32, 0x11000000, 0x11007FFF, vu, vu0);
    MAP_MEM_READ_NS(32, 0x11008000, 0x1100FFFF, vu, vu1);
    MAP_MEM_READ_NS(32, 0x1E000000, 0x1E3FFFFF, bios, rom1);
    MAP_MEM_READ_NS(32, 0x1E400000, 0x1E7FFFFF, bios, rom2);
    MAP_REG_READ_NS(32, 0x1F801600, 0x1F8016FF, usb, usb);
    MAP_REG_READ_NS(32, 0x1F801460, 0x1F80147F, dev9, dev9);
    MAP_REG_READ_NS(32, 0x14000000, 0x1400FFFF, speed, speed);

    switch (addr) {
        case 0x1000F440: {
            uint8_t sop = (bus->mch_ricm >> 6) & 0xF;
            uint8_t sa = (bus->mch_ricm >> 16) & 0xFFF;

            if (!sop) {
                switch (sa) {
                    case 0x21: {
                        if (bus->rdram_sdevid < 2) {
                            bus->rdram_sdevid++;

                            return 0x1F;
                        }

                        return 0;
                    } break;

                    case 0x23: return 0x0D0D;
                    case 0x24: return 0x0090;
                    case 0x40: return bus->mch_ricm & 0x1F;
                }
            }

            return 0;
        } break;
        case 0x1000f130:
        case 0x1000f400:
        case 0x1000f410:
        case 0x1000f430:
        case 0x1f80141c: {
            return 0;
        } break;
    }

    // printf("bus: Unhandled 32-bit read from physical address 0x%08x\n", addr);
    
    if ((addr & 0xffff0000) == 0xfffe0000)
        exit(1);

    return 0;
}

uint64_t read64(void* udata, uint32_t addr) {
    Bus* bus = (Bus*)udata;

    void* ptr = fastmem_read_ptr(bus, addr);

    if (likely(ptr)) return *((uint64_t*)(((uint8_t*)ptr) + (addr & 0x1fff)));

    // MAP_MEM_READ_NS(64, 0x00000000, 0x01FFFFFF, ram, ee_ram);
    // MAP_MEM_READ_NS(64, 0x20000000, 0x21FFFFFF, ram, ee_ram);
    // MAP_MEM_READ_NS(64, 0x30000000, 0x31FFFFFF, ram, ee_ram);
    // MAP_MEM_READ_NS(64, 0x1C000000, 0x1C1FFFFF, ram, iop_ram);
    // MAP_MEM_READ_NS(64, 0x1FC00000, 0x1FFFFFFF, bios, bios);
    MAP_REG_READ_NS(64, 0x12000000, 0x12001FFF, gs, gs);
    MAP_REG_READ_NS(64, 0x10002000, 0x1000203F, ipu, ipu);
    MAP_REG_READ_NS(64, 0x10007000, 0x1000701F, ipu, ipu);
    MAP_REG_READ_NS(32, 0x10008000, 0x1000EFFF, dmac, dmac);
    MAP_REG_READ_NS(32, 0x1000F520, 0x1000F5FF, dmac, dmac);
    MAP_REG_READ_NS(32, 0x10000000, 0x10001FFF, timers, timers); // Reuse 32-bit function
    MAP_MEM_READ_NS(64, 0x11000000, 0x11007FFF, vu, vu0);
    MAP_MEM_READ_NS(64, 0x11008000, 0x1100FFFF, vu, vu1);
    MAP_MEM_READ_NS(64, 0x1E000000, 0x1E3FFFFF, bios, rom1);
    MAP_MEM_READ_NS(64, 0x1E400000, 0x1E7FFFFF, bios, rom2);

    iris_error(bus, "Unhandled 64-bit read from physical address {:08x}", addr);

    return 0;
}

uint128_t read128(void* udata, uint32_t addr) {
    Bus* bus = (Bus*)udata;

    void* ptr = fastmem_read_ptr(bus, addr);

    if (likely(ptr)) return *((uint128_t*)(((uint8_t*)ptr) + (addr & 0x1fff)));

    // MAP_MEM_READ_NS(128, 0x00000000, 0x01FFFFFF, ram, ee_ram);
    // MAP_MEM_READ_NS(128, 0x20000000, 0x21FFFFFF, ram, ee_ram);
    // MAP_MEM_READ_NS(128, 0x30000000, 0x31FFFFFF, ram, ee_ram);
    // MAP_MEM_READ_NS(128, 0x1C000000, 0x1C1FFFFF, ram, iop_ram);
    // MAP_MEM_READ_NS(128, 0x1FC00000, 0x1FFFFFFF, bios, bios);
    MAP_REG_READ_NS(128, 0x10004000, 0x10004FFF, vif, vif0);
    MAP_REG_READ_NS(128, 0x10005000, 0x10005FFF, vif, vif1);
    MAP_REG_READ_NS(128, 0x10007000, 0x1000701F, ipu, ipu);
    MAP_MEM_READ_NS(128, 0x11000000, 0x11007FFF, vu, vu0);
    MAP_MEM_READ_NS(128, 0x11008000, 0x1100FFFF, vu, vu1);
    MAP_MEM_READ_NS(128, 0x1E000000, 0x1E3FFFFF, bios, rom1);
    MAP_MEM_READ_NS(128, 0x1E400000, 0x1E7FFFFF, bios, rom2);

    // fprintf(stderr, "bus: Unhandled 128-bit read from physical address 0x%08x\n", addr); // exit(1);

    // *(int*)0 = 0;

    return uint128_t{};
}

void write8(void* udata, uint32_t addr, uint64_t data) {
    Bus* bus = (Bus*)udata;

    void* ptr = fastmem_write_ptr(bus, addr);

    if (likely(ptr)) {
        *((uint8_t*)(((uint8_t*)ptr) + (addr & 0x1fff))) = data;

        return;
    }

    // MAP_MEM_WRITE_NS(8, 0x00000000, 0x01FFFFFF, ram, ee_ram);
    // MAP_MEM_WRITE_NS(8, 0x20000000, 0x21FFFFFF, ram, ee_ram);
    // MAP_MEM_WRITE_NS(8, 0x30000000, 0x31FFFFFF, ram, ee_ram);
    // MAP_MEM_WRITE_NS(8, 0x1C000000, 0x1C1FFFFF, ram, iop_ram);
    // MAP_MEM_WRITE_NS(8, 0x1FC00000, 0x1FFFFFFF, bios, bios); // BIOS Firmware update
    MAP_REG_WRITE_NS(8, 0x10008000, 0x1000EFFF, dmac, dmac);
    MAP_REG_WRITE_NS(8, 0x1000F520, 0x1000F5FF, dmac, dmac);
    MAP_REG_WRITE_NS(8, 0x1F402004, 0x1F402018, cdvd, cdvd);
    MAP_MEM_WRITE_NS(8, 0x11000000, 0x11007FFF, vu, vu0);
    MAP_MEM_WRITE_NS(8, 0x11008000, 0x1100FFFF, vu, vu1);
    MAP_REG_WRITE_NS(8, 0x1000F000, 0x1000F01F, intc, intc);
    MAP_REG_WRITE_NS(8, 0x1F801460, 0x1F80147F, dev9, dev9);
    MAP_REG_WRITE_NS(8, 0x14000000, 0x1400FFFF, speed, speed);

    if (addr >= 0x1C000000 && addr < 0x1C000000 + bus->iop_ram->size) {
        ram::write8(bus->iop_ram, addr - 0x1C000000, data & 0xFF);

        iop::invalidate_block(bus->iop, addr - 0x1C000000);

        return;
    }

    if (addr == 0x1000f180) { bus->kputchar(bus->kputchar_udata, data & 0xff); return; }

    // printf("bus: Unhandled 8-bit write to physical address 0x%08x (0x%02lx)\n", addr, data);
}

void write16(void* udata, uint32_t addr, uint64_t data) {
    Bus* bus = (Bus*)udata;

    void* ptr = fastmem_write_ptr(bus, addr);

    if (likely(ptr)) {
        *((uint16_t*)(((uint8_t*)ptr) + (addr & 0x1fff))) = data;

        return;
    }

    // MAP_MEM_WRITE_NS(16, 0x00000000, 0x01FFFFFF, ram, ee_ram);
    // MAP_MEM_WRITE_NS(16, 0x20000000, 0x21FFFFFF, ram, ee_ram);
    // MAP_MEM_WRITE_NS(16, 0x30000000, 0x31FFFFFF, ram, ee_ram);
    // MAP_MEM_WRITE_NS(16, 0x1C000000, 0x1C1FFFFF, ram, iop_ram);
    // MAP_MEM_WRITE_NS(16, 0x1FC00000, 0x1FFFFFFF, bios, bios);
    MAP_REG_WRITE_NS(16, 0x10008000, 0x1000EFFF, dmac, dmac);
    MAP_REG_WRITE_NS(16, 0x1000F520, 0x1000F5FF, dmac, dmac);
    MAP_MEM_WRITE_NS(16, 0x11000000, 0x11007FFF, vu, vu0);
    MAP_MEM_WRITE_NS(16, 0x11008000, 0x1100FFFF, vu, vu1);
    MAP_REG_WRITE_NS(16, 0x1000F000, 0x1000F01F, intc, intc);
    MAP_REG_WRITE_NS(16, 0x10000000, 0x10001FFF, timers, timers);
    MAP_REG_WRITE_NS(16, 0x1F801460, 0x1F80147F, dev9, dev9);
    MAP_REG_WRITE_NS(16, 0x14000000, 0x1400FFFF, speed, speed);

    if (addr >= 0x1C000000 && addr < 0x1C000000 + bus->iop_ram->size) {
        ram::write16(bus->iop_ram, addr - 0x1C000000, data & 0xFFFF);

        iop::invalidate_block(bus->iop, addr - 0x1C000000);

        return;
    }

    switch (addr) {
        case 0x1a000008:
        case 0x1f801470:
        case 0x1f801472: return;
    }

    // printf("bus: Unhandled 16-bit write to physical address 0x%08x (0x%04lx)\n", addr, data);
}

void write32(void* udata, uint32_t addr, uint64_t data) {
    Bus* bus = (Bus*)udata;

    void* ptr = fastmem_write_ptr(bus, addr);

    if (likely(ptr)) {
        *((uint32_t*)(((uint8_t*)ptr) + (addr & 0x1fff))) = data;

        return;
    }

    // MAP_MEM_WRITE_NS(32, 0x00000000, 0x01FFFFFF, ram, ee_ram);
    // MAP_MEM_WRITE_NS(32, 0x20000000, 0x21FFFFFF, ram, ee_ram);
    // MAP_MEM_WRITE_NS(32, 0x30000000, 0x31FFFFFF, ram, ee_ram);
    // MAP_MEM_WRITE_NS(32, 0x1C000000, 0x1C1FFFFF, ram, iop_ram);
    // MAP_MEM_WRITE_NS(32, 0x1FC00000, 0x1FFFFFFF, bios, bios);
    MAP_REG_WRITE_NS(32, 0x10000000, 0x10001FFF, timers, timers);
    MAP_REG_WRITE_NS(64, 0x10002000, 0x1000203F, ipu, ipu);
    MAP_REG_WRITE_NS(32, 0x10003000, 0x100037FF, gif, gif);
    MAP_REG_WRITE_NS(64, 0x10007000, 0x1000701F, ipu, ipu);
    MAP_REG_WRITE_NS(32, 0x10008000, 0x1000EFFF, dmac, dmac);
    MAP_REG_WRITE_NS(32, 0x1000F000, 0x1000F01F, intc, intc);
    MAP_REG_WRITE_NS(32, 0x1000F200, 0x1000F26F, sif, sif);
    MAP_REG_WRITE_NS(32, 0x10003800, 0x10003BFF, vif, vif0);
    MAP_REG_WRITE_NS(32, 0x10003C00, 0x10003FFF, vif, vif1);
    MAP_REG_WRITE_NS(32, 0x10004000, 0x10004FFF, vif, vif0);
    MAP_REG_WRITE_NS(32, 0x10005000, 0x10005FFF, vif, vif1);
    MAP_REG_WRITE_NS(32, 0x1000F520, 0x1000F5FF, dmac, dmac);
    MAP_REG_WRITE_NS(64, 0x12000000, 0x12001FFF, gs, gs); // Reuse 64-bit function
    MAP_MEM_WRITE_NS(32, 0x11000000, 0x11007FFF, vu, vu0);
    MAP_MEM_WRITE_NS(32, 0x11008000, 0x1100FFFF, vu, vu1);
    MAP_REG_WRITE_NS(32, 0x1F801600, 0x1F8016FF, usb, usb);
    MAP_REG_WRITE_NS(32, 0x1F801460, 0x1F80147F, dev9, dev9);
    MAP_REG_WRITE_NS(32, 0x14000000, 0x1400FFFF, speed, speed);

    if (addr >= 0x1C000000 && addr < 0x1C000000 + bus->iop_ram->size) {
        ram::write32(bus->iop_ram, addr - 0x1C000000, data & 0xFFFFFFFF);

        iop::invalidate_block(bus->iop, addr - 0x1C000000);

        return;
    }

    switch (addr) {
        case 0x1000f430: {
            uint8_t sa = (data >> 16) & 0xFFF;
            uint8_t sbc = (data >> 6) & 0xF;

            if ((sa == 0x21) && (sbc == 0x1) && ((bus->mch_drd >> 7) & 1) == 0)
                bus->rdram_sdevid = 0;

            bus->mch_ricm = data & ~0x80000000;
        } return;
        case 0x1000f440: {
            bus->mch_drd = data;
        } return;
        case 0x1000f100:
        case 0x1000f120:
        case 0x1000f140:
        case 0x1000f150:
        case 0x1000f400:
        case 0x1000f410:
        case 0x1000f420:
        case 0x1000f450:
        case 0x1000f460:
        case 0x1000f480:
        case 0x1000f490:
        case 0x1000f500:
        case 0x1000f510:
        case 0x1f80141c: return;
    }

    // fprintf(stderr, "bus: Unhandled 32-bit write to physical address 0x%08x (0x%08lx)\n", addr, data); if ((addr & 0xff000000) == 0x02000000) exit(1);
}

void write64(void* udata, uint32_t addr, uint64_t data) {
    Bus* bus = (Bus*)udata;

    void* ptr = fastmem_write_ptr(bus, addr);

    if (likely(ptr)) {
        *((uint64_t*)(((uint8_t*)ptr) + (addr & 0x1fff))) = data;

        return;
    }

    // MAP_MEM_WRITE_NS(64, 0x00000000, 0x01FFFFFF, ram, ee_ram);
    // MAP_MEM_WRITE_NS(64, 0x20000000, 0x21FFFFFF, ram, ee_ram);
    // MAP_MEM_WRITE_NS(64, 0x30000000, 0x31FFFFFF, ram, ee_ram);
    // MAP_MEM_WRITE_NS(64, 0x1C000000, 0x1C1FFFFF, ram, iop_ram);
    // MAP_MEM_WRITE_NS(64, 0x1FC00000, 0x1FFFFFFF, bios, bios);
    MAP_REG_WRITE_NS(64, 0x12000000, 0x12002000, gs, gs);
    MAP_REG_WRITE_NS(64, 0x10002000, 0x1000203F, ipu, ipu);
    MAP_REG_WRITE_NS(64, 0x10007000, 0x1000701F, ipu, ipu);
    MAP_REG_WRITE_NS(32, 0x10008000, 0x1000EFFF, dmac, dmac);
    MAP_REG_WRITE_NS(32, 0x1000F520, 0x1000F5FF, dmac, dmac);
    MAP_REG_WRITE_NS(32, 0x10000000, 0x10001FFF, timers, timers); // Reuse 32-bit function
    MAP_MEM_WRITE_NS(64, 0x11000000, 0x11007FFF, vu, vu0);
    MAP_MEM_WRITE_NS(64, 0x11008000, 0x1100FFFF, vu, vu1);
    MAP_MEM_WRITE_NS(64, 0x1000F000, 0x1000F01F, intc, intc);

    if (addr >= 0x1C000000 && addr < 0x1C000000 + bus->iop_ram->size) {
        ram::write64(bus->iop_ram, addr - 0x1C000000, data);

        iop::invalidate_block(bus->iop, addr - 0x1C000000);

        return;
    }

    iris_error(bus, "Unhandled 64-bit write to physical address {:08x} ({:016x})", addr, data);
}

void write128(void* udata, uint32_t addr, uint128_t data) {
    Bus* bus = (Bus*)udata;

    void* ptr = fastmem_write_ptr(bus, addr);

    if (likely(ptr)) {
        *((uint128_t*)(((uint8_t*)ptr) + (addr & 0x1fff))) = data;

        return;
    }

    // MAP_MEM_WRITE_NS(128, 0x00000000, 0x01FFFFFF, ram, ee_ram);
    // MAP_MEM_WRITE_NS(128, 0x20000000, 0x21FFFFFF, ram, ee_ram);
    // MAP_MEM_WRITE_NS(128, 0x30000000, 0x31FFFFFF, ram, ee_ram);
    // MAP_MEM_WRITE_NS(128, 0x1C000000, 0x1C1FFFFF, ram, iop_ram);
    // MAP_MEM_WRITE_NS(128, 0x1FC00000, 0x1FFFFFFF, bios, bios);
    MAP_REG_WRITE_NS(128, 0x10006000, 0x10006FFF, gif, gif);
    MAP_REG_WRITE_NS(128, 0x10007000, 0x1000701F, ipu, ipu);
    MAP_REG_WRITE_NS(128, 0x10004000, 0x10004FFF, vif, vif0);
    MAP_REG_WRITE_NS(128, 0x10005000, 0x10005FFF, vif, vif1);
    MAP_MEM_WRITE_NS(128, 0x11000000, 0x11007FFF, vu, vu0);
    MAP_MEM_WRITE_NS(128, 0x11008000, 0x1100FFFF, vu, vu1);

    if (addr >= 0x1C000000 && addr < 0x1C000000 + bus->iop_ram->size) {
        ram::write128(bus->iop_ram, addr - 0x1C000000, data);

        iop::invalidate_block(bus->iop, addr - 0x1C000000);

        return;
    }

    // printf("bus: Unhandled 128-bit write to physical address 0x%08x (0x%08x%08x%08x%08x)\n", addr, data.u32[3], data.u32[2], data.u32[1], data.u32[0]);
}

}
