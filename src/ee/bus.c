#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "bus.h"
#include "bus_decl.h"

static void* fastmem_r_table[0x10000] = {
    NULL
};

static void* fastmem_w_table[0x10000] = {
    NULL
};

struct ee_bus* ee_bus_create(void) {
    return malloc(sizeof(struct ee_bus));
}

void ee_bus_init(struct ee_bus* bus, const char* bios_path) {
    memset(bus, 0, sizeof(struct ee_bus));

    for (int i = 0; i < 0x10000; i++) {
        fastmem_r_table[i] = NULL;
        fastmem_w_table[i] = NULL;
    }
}

void ee_bus_init_fastmem(struct ee_bus* bus) {
    // BIOS
    for (int i = 0; i < 0x200; i++) {
        fastmem_r_table[i+0xfe00] = bus->bios->buf + (i * 0x2000);
        fastmem_w_table[i+0xfe00] = NULL;
    }

    // Main RAM
    for (int i = 0; i < 0x1000; i++) {
        fastmem_r_table[i+0x0000] = bus->ee_ram->buf + (i * 0x2000);
        fastmem_w_table[i+0x0000] = bus->ee_ram->buf + (i * 0x2000);
    }

    // IOP RAM
    for (int i = 0; i < 0x100; i++) {
        fastmem_r_table[i+0xe000] = bus->iop_ram->buf + (i * 0x2000);
        fastmem_w_table[i+0xe000] = bus->iop_ram->buf + (i * 0x2000);
    }
}

void ee_bus_init_bios(struct ee_bus* bus, struct ps2_bios* bios) {
    bus->bios = bios;
}

void ee_bus_init_rom1(struct ee_bus* bus, struct ps2_bios* rom1) {
    bus->rom1 = rom1;
}

void ee_bus_init_rom2(struct ee_bus* bus, struct ps2_bios* rom2) {
    bus->rom2 = rom2;
}

void ee_bus_init_iop_ram(struct ee_bus* bus, struct ps2_ram* iop_ram) {
    bus->iop_ram = iop_ram;
}

void ee_bus_init_sif(struct ee_bus* bus, struct ps2_sif* sif) {
    bus->sif = sif;
}

void ee_bus_init_ram(struct ee_bus* bus, struct ps2_ram* ram) {
    bus->ee_ram = ram;
}

void ee_bus_init_dmac(struct ee_bus* bus, struct ps2_dmac* dmac) {
    bus->dmac = dmac;
}

void ee_bus_init_intc(struct ee_bus* bus, struct ps2_intc* intc) {
    bus->intc = intc;
}

void ee_bus_init_gif(struct ee_bus* bus, struct ps2_gif* gif) {
    bus->gif = gif;
}

void ee_bus_init_vif(struct ee_bus* bus, struct ps2_vif* vif) {
    bus->vif = vif;
}

void ee_bus_init_gs(struct ee_bus* bus, struct ps2_gs* gs) {
    bus->gs = gs;
}

void ee_bus_init_ipu(struct ee_bus* bus, struct ps2_ipu* ipu) {
    bus->ipu = ipu;
}

void ee_bus_init_timers(struct ee_bus* bus, struct ps2_ee_timers* timers) {
    bus->timers = timers;
}

void ee_bus_init_cdvd(struct ee_bus* bus, struct ps2_cdvd* cdvd) {
    bus->cdvd = cdvd;
}

void ee_bus_init_usb(struct ee_bus* bus, struct ps2_usb* usb) {
    bus->usb = usb;
}

void ee_bus_init_vu0(struct ee_bus* bus, struct vu_state* vu) {
    bus->vu0 = vu;
}

void ee_bus_init_vu1(struct ee_bus* bus, struct vu_state* vu) {
    bus->vu1 = vu;
}

void ee_bus_init_kputchar(struct ee_bus* bus, void (*kputchar)(void*, char), void* udata) {
    bus->kputchar = kputchar;
    bus->kputchar_udata = udata;
}

void ee_bus_destroy(struct ee_bus* bus) {
    free(bus);
}

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

uint64_t ee_bus_read8(void* udata, uint32_t addr) {
    struct ee_bus* bus = (struct ee_bus*)udata;

    void* ptr = fastmem_r_table[addr >> 13];

    if (ptr) return *((uint8_t*)(((uint8_t*)ptr) + (addr & 0x1fff)));

    MAP_MEM_READ(8, 0x00000000, 0x01FFFFFF, ram, ee_ram);
    MAP_MEM_READ(8, 0x20000000, 0x21FFFFFF, ram, ee_ram);
    MAP_MEM_READ(8, 0x30000000, 0x31FFFFFF, ram, ee_ram);
    MAP_MEM_READ(8, 0x1C000000, 0x1C1FFFFF, ram, iop_ram);
    MAP_MEM_READ(8, 0x1FC00000, 0x1FFFFFFF, bios, bios);
    MAP_MEM_READ(8, 0x11000000, 0x11007FFF, vu, vu0);
    MAP_MEM_READ(8, 0x11008000, 0x1100FFFF, vu, vu1);
    MAP_REG_READ(8, 0x10008000, 0x1000EFFF, dmac, dmac);
    MAP_REG_READ(8, 0x1000F520, 0x1000F5FF, dmac, dmac);
    MAP_REG_READ(8, 0x1F402004, 0x1F402018, cdvd, cdvd);
    MAP_MEM_READ(8, 0x1E000000, 0x1E3FFFFF, bios, rom1);
    MAP_MEM_READ(8, 0x1E400000, 0x1E7FFFFF, bios, rom2);

    printf("bus: Unhandled 8-bit read from physical address 0x%08x\n", addr);

    return 0;
}

uint64_t ee_bus_read16(void* udata, uint32_t addr) {
    struct ee_bus* bus = (struct ee_bus*)udata;

    void* ptr = fastmem_r_table[addr >> 13];

    if (ptr) return *((uint16_t*)(((uint8_t*)ptr) + (addr & 0x1fff)));

    MAP_MEM_READ(16, 0x00000000, 0x01FFFFFF, ram, ee_ram);
    MAP_MEM_READ(16, 0x20000000, 0x21FFFFFF, ram, ee_ram);
    MAP_MEM_READ(16, 0x1FC00000, 0x1FFFFFFF, bios, bios);
    MAP_MEM_READ(16, 0x30000000, 0x31FFFFFF, ram, ee_ram);
    MAP_MEM_READ(16, 0x1C000000, 0x1C1FFFFF, ram, iop_ram);
    MAP_MEM_READ(16, 0x11000000, 0x11007FFF, vu, vu0);
    MAP_MEM_READ(16, 0x11008000, 0x1100FFFF, vu, vu1);
    MAP_MEM_READ(16, 0x1E000000, 0x1E3FFFFF, bios, rom1);
    MAP_MEM_READ(16, 0x1E400000, 0x1E7FFFFF, bios, rom2);

    if (addr == 0x1a000010) return 0xffff;

    switch (addr) {
        case 0x1f803800: return 0;

        // SCPH-39001 stub
        case 0x1a000006: return 2;
    }

    printf("bus: Unhandled 16-bit read from physical address 0x%08x\n", addr); // exit(1);

    return 0;
}

uint64_t ee_bus_read32(void* udata, uint32_t addr) {
    struct ee_bus* bus = (struct ee_bus*)udata;

    void* ptr = fastmem_r_table[addr >> 13];

    if (ptr) return *((uint32_t*)(((uint8_t*)ptr) + (addr & 0x1fff)));

    MAP_MEM_READ(32, 0x00000000, 0x01FFFFFF, ram, ee_ram);
    MAP_MEM_READ(32, 0x20000000, 0x21FFFFFF, ram, ee_ram);
    MAP_MEM_READ(32, 0x30000000, 0x31FFFFFF, ram, ee_ram);
    MAP_MEM_READ(32, 0x1C000000, 0x1C1FFFFF, ram, iop_ram);
    MAP_MEM_READ(32, 0x1FC00000, 0x1FFFFFFF, bios, bios);
    MAP_REG_READ(32, 0x1000F200, 0x1000F26F, sif, sif);
    MAP_REG_READ(32, 0x10008000, 0x1000EFFF, dmac, dmac);
    MAP_REG_READ(32, 0x1000F520, 0x1000F5FF, dmac, dmac);
    MAP_REG_READ(64, 0x10002000, 0x1000203F, ipu, ipu);
    MAP_REG_READ(64, 0x10007000, 0x1000701F, ipu, ipu);
    MAP_REG_READ(32, 0x10003000, 0x100037FF, gif, gif);
    MAP_REG_READ(32, 0x10003800, 0x10005FFF, vif, vif);
    MAP_REG_READ(32, 0x1000F000, 0x1000F01F, intc, intc);
    MAP_REG_READ(64, 0x12000000, 0x12001FFF, gs, gs); // Reuse 64-bit function
    MAP_REG_READ(32, 0x10000000, 0x10001FFF, ee_timers, timers);
    MAP_MEM_READ(32, 0x11000000, 0x11007FFF, vu, vu0);
    MAP_MEM_READ(32, 0x11008000, 0x1100FFFF, vu, vu1);
    MAP_MEM_READ(32, 0x1E000000, 0x1E3FFFFF, bios, rom1);
    MAP_MEM_READ(32, 0x1E400000, 0x1E7FFFFF, bios, rom2);
    MAP_REG_READ(32, 0x1F801600, 0x1F8016FF, usb, usb);

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

    printf("bus: Unhandled 32-bit read from physical address 0x%08x\n", addr);
    
    if ((addr & 0xffff0000) == 0xfffe0000)
        exit(1);

    return 0;
}

uint64_t ee_bus_read64(void* udata, uint32_t addr) {
    struct ee_bus* bus = (struct ee_bus*)udata;

    void* ptr = fastmem_r_table[addr >> 13];

    if (ptr) return *((uint64_t*)(((uint8_t*)ptr) + (addr & 0x1fff)));

    MAP_MEM_READ(64, 0x00000000, 0x01FFFFFF, ram, ee_ram);
    MAP_MEM_READ(64, 0x20000000, 0x21FFFFFF, ram, ee_ram);
    MAP_MEM_READ(64, 0x30000000, 0x31FFFFFF, ram, ee_ram);
    MAP_MEM_READ(64, 0x1C000000, 0x1C1FFFFF, ram, iop_ram);
    MAP_MEM_READ(64, 0x1FC00000, 0x1FFFFFFF, bios, bios);
    MAP_REG_READ(64, 0x12000000, 0x12001FFF, gs, gs);
    MAP_REG_READ(64, 0x10002000, 0x1000203F, ipu, ipu);
    MAP_REG_READ(64, 0x10007000, 0x1000701F, ipu, ipu);
    MAP_REG_READ(32, 0x10008000, 0x1000EFFF, dmac, dmac);
    MAP_REG_READ(32, 0x1000F520, 0x1000F5FF, dmac, dmac);
    MAP_REG_READ(32, 0x10000000, 0x10001FFF, ee_timers, timers); // Reuse 32-bit function
    MAP_MEM_READ(64, 0x11000000, 0x11007FFF, vu, vu0);
    MAP_MEM_READ(64, 0x11008000, 0x1100FFFF, vu, vu1);
    MAP_MEM_READ(64, 0x1E000000, 0x1E3FFFFF, bios, rom1);
    MAP_MEM_READ(64, 0x1E400000, 0x1E7FFFFF, bios, rom2);

    printf("bus: Unhandled 64-bit read from physical address 0x%08x\n", addr); // exit(1);

    return 0;
}

uint128_t ee_bus_read128(void* udata, uint32_t addr) {
    struct ee_bus* bus = (struct ee_bus*)udata;

    void* ptr = fastmem_r_table[addr >> 13];

    if (ptr) return *((uint128_t*)(((uint8_t*)ptr) + (addr & 0x1fff)));

    MAP_MEM_READ(128, 0x00000000, 0x01FFFFFF, ram, ee_ram);
    MAP_MEM_READ(128, 0x20000000, 0x21FFFFFF, ram, ee_ram);
    MAP_MEM_READ(128, 0x30000000, 0x31FFFFFF, ram, ee_ram);
    MAP_MEM_READ(128, 0x1C000000, 0x1C1FFFFF, ram, iop_ram);
    MAP_MEM_READ(128, 0x1FC00000, 0x1FFFFFFF, bios, bios);
    MAP_REG_READ(128, 0x10004000, 0x10005FFF, vif, vif);
    MAP_REG_READ(128, 0x10007000, 0x1000701F, ipu, ipu);
    MAP_MEM_READ(128, 0x11000000, 0x11007FFF, vu, vu0);
    MAP_MEM_READ(128, 0x11008000, 0x1100FFFF, vu, vu1);
    MAP_MEM_READ(128, 0x1E000000, 0x1E3FFFFF, bios, rom1);
    MAP_MEM_READ(128, 0x1E400000, 0x1E7FFFFF, bios, rom2);

    printf("bus: Unhandled 128-bit read from physical address 0x%08x\n", addr); // exit(1);

    return (uint128_t){ .u64[0] = 0, .u64[1] = 0 };
}

void ee_bus_write8(void* udata, uint32_t addr, uint64_t data) {
    struct ee_bus* bus = (struct ee_bus*)udata;

    void* ptr = fastmem_w_table[addr >> 13];

    if (ptr) {
        *((uint8_t*)(((uint8_t*)ptr) + (addr & 0x1fff))) = data;

        return;
    }

    MAP_MEM_WRITE(8, 0x00000000, 0x01FFFFFF, ram, ee_ram);
    MAP_MEM_WRITE(8, 0x20000000, 0x21FFFFFF, ram, ee_ram);
    MAP_MEM_WRITE(8, 0x30000000, 0x31FFFFFF, ram, ee_ram);
    MAP_MEM_WRITE(8, 0x1C000000, 0x1C1FFFFF, ram, iop_ram);
    MAP_REG_WRITE(8, 0x10008000, 0x1000EFFF, dmac, dmac);
    MAP_REG_WRITE(8, 0x1000F520, 0x1000F5FF, dmac, dmac);
    MAP_REG_WRITE(8, 0x1F402004, 0x1F402018, cdvd, cdvd);
    MAP_MEM_WRITE(8, 0x11000000, 0x11007FFF, vu, vu0);
    MAP_MEM_WRITE(8, 0x11008000, 0x1100FFFF, vu, vu1);
    MAP_REG_WRITE(8, 0x1000F000, 0x1000F01F, intc, intc);
    MAP_MEM_WRITE(8, 0x1FC00000, 0x1FFFFFFF, bios, bios); // BIOS Firmware update

    if (addr == 0x1000f180) { bus->kputchar(bus->kputchar_udata, data & 0xff); return; }

    printf("bus: Unhandled 8-bit write to physical address 0x%08x (0x%02lx)\n", addr, data);
}

void ee_bus_write16(void* udata, uint32_t addr, uint64_t data) {
    struct ee_bus* bus = (struct ee_bus*)udata;

    void* ptr = fastmem_w_table[addr >> 13];

    if (ptr) {
        *((uint16_t*)(((uint8_t*)ptr) + (addr & 0x1fff))) = data;

        return;
    }

    MAP_MEM_WRITE(16, 0x00000000, 0x01FFFFFF, ram, ee_ram);
    MAP_MEM_WRITE(16, 0x20000000, 0x21FFFFFF, ram, ee_ram);
    MAP_MEM_WRITE(16, 0x30000000, 0x31FFFFFF, ram, ee_ram);
    MAP_MEM_WRITE(16, 0x1C000000, 0x1C1FFFFF, ram, iop_ram);
    MAP_MEM_WRITE(16, 0x11000000, 0x11007FFF, vu, vu0);
    MAP_MEM_WRITE(16, 0x11008000, 0x1100FFFF, vu, vu1);
    MAP_REG_WRITE(16, 0x1000F000, 0x1000F01F, intc, intc);
    MAP_MEM_WRITE(16, 0x1FC00000, 0x1FFFFFFF, bios, bios);

    switch (addr) {
        case 0x1a000008:
        case 0x1f801470:
        case 0x1f801472: return;
    }

    printf("bus: Unhandled 16-bit write to physical address 0x%08x (0x%04lx)\n", addr, data); // exit(1);
}

void ee_bus_write32(void* udata, uint32_t addr, uint64_t data) {
    struct ee_bus* bus = (struct ee_bus*)udata;

    void* ptr = fastmem_w_table[addr >> 13];

    if (ptr) {
        *((uint32_t*)(((uint8_t*)ptr) + (addr & 0x1fff))) = data;

        return;
    }

    MAP_MEM_WRITE(32, 0x00000000, 0x01FFFFFF, ram, ee_ram);
    MAP_MEM_WRITE(32, 0x20000000, 0x21FFFFFF, ram, ee_ram);
    MAP_MEM_WRITE(32, 0x30000000, 0x31FFFFFF, ram, ee_ram);
    MAP_REG_WRITE(32, 0x10000000, 0x10001FFF, ee_timers, timers);
    MAP_REG_WRITE(64, 0x10002000, 0x1000203F, ipu, ipu);
    MAP_REG_WRITE(32, 0x10003000, 0x100037FF, gif, gif);
    MAP_REG_WRITE(64, 0x10007000, 0x1000701F, ipu, ipu);
    MAP_REG_WRITE(32, 0x10008000, 0x1000EFFF, dmac, dmac);
    MAP_REG_WRITE(32, 0x1000F000, 0x1000F01F, intc, intc);
    MAP_REG_WRITE(32, 0x1000F200, 0x1000F26F, sif, sif);
    MAP_REG_WRITE(32, 0x10003800, 0x10005FFF, vif, vif);
    MAP_REG_WRITE(32, 0x1000F520, 0x1000F5FF, dmac, dmac);
    MAP_REG_WRITE(64, 0x12000000, 0x12001FFF, gs, gs); // Reuse 64-bit function
    MAP_MEM_WRITE(32, 0x1C000000, 0x1C1FFFFF, ram, iop_ram);
    MAP_MEM_WRITE(32, 0x11000000, 0x11007FFF, vu, vu0);
    MAP_MEM_WRITE(32, 0x11008000, 0x1100FFFF, vu, vu1);
    MAP_MEM_WRITE(32, 0x1FC00000, 0x1FFFFFFF, bios, bios);
    MAP_REG_WRITE(32, 0x1F801600, 0x1F8016FF, usb, usb);

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

    printf("bus: Unhandled 32-bit write to physical address 0x%08x (0x%08lx)\n", addr, data); if ((addr & 0xff000000) == 0x02000000) exit(1);
}

void ee_bus_write64(void* udata, uint32_t addr, uint64_t data) {
    struct ee_bus* bus = (struct ee_bus*)udata;

    void* ptr = fastmem_w_table[addr >> 13];

    if (ptr) {
        *((uint64_t*)(((uint8_t*)ptr) + (addr & 0x1fff))) = data;

        return;
    }

    MAP_MEM_WRITE(64, 0x00000000, 0x01FFFFFF, ram, ee_ram);
    MAP_MEM_WRITE(64, 0x20000000, 0x21FFFFFF, ram, ee_ram);
    MAP_MEM_WRITE(64, 0x30000000, 0x31FFFFFF, ram, ee_ram);
    MAP_MEM_WRITE(64, 0x1C000000, 0x1C1FFFFF, ram, iop_ram);
    MAP_REG_WRITE(64, 0x12000000, 0x12002000, gs, gs);
    MAP_REG_WRITE(64, 0x10002000, 0x1000203F, ipu, ipu);
    MAP_REG_WRITE(64, 0x10007000, 0x1000701F, ipu, ipu);
    MAP_REG_WRITE(32, 0x10008000, 0x1000EFFF, dmac, dmac);
    MAP_REG_WRITE(32, 0x1000F520, 0x1000F5FF, dmac, dmac);
    MAP_MEM_WRITE(64, 0x11000000, 0x11007FFF, vu, vu0);
    MAP_MEM_WRITE(64, 0x11008000, 0x1100FFFF, vu, vu1);
    MAP_MEM_WRITE(64, 0x1000F000, 0x1000F01F, intc, intc);
    MAP_MEM_WRITE(64, 0x1FC00000, 0x1FFFFFFF, bios, bios);

    // printf("bus: Unhandled 64-bit write to physical address 0x%08x (0x%08lx%08lx)\n", addr, data >> 32, data & 0xffffffff);
}

void ee_bus_write128(void* udata, uint32_t addr, uint128_t data) {
    struct ee_bus* bus = (struct ee_bus*)udata;

    void* ptr = fastmem_w_table[addr >> 13];

    if (ptr) {
        *((uint128_t*)(((uint8_t*)ptr) + (addr & 0x1fff))) = data;

        return;
    }

    MAP_MEM_WRITE(128, 0x00000000, 0x01FFFFFF, ram, ee_ram);
    MAP_MEM_WRITE(128, 0x20000000, 0x21FFFFFF, ram, ee_ram);
    MAP_MEM_WRITE(128, 0x30000000, 0x31FFFFFF, ram, ee_ram);
    MAP_MEM_WRITE(128, 0x1C000000, 0x1C1FFFFF, ram, iop_ram);
    MAP_REG_WRITE(128, 0x10006000, 0x10006FFF, gif, gif);
    MAP_REG_WRITE(128, 0x10007000, 0x1000701F, ipu, ipu);
    MAP_REG_WRITE(128, 0x10004000, 0x10005FFF, vif, vif);
    MAP_MEM_WRITE(128, 0x11000000, 0x11007FFF, vu, vu0);
    MAP_MEM_WRITE(128, 0x11008000, 0x1100FFFF, vu, vu1);
    MAP_MEM_WRITE(128, 0x1FC00000, 0x1FFFFFFF, bios, bios);

    // printf("bus: Unhandled 128-bit write to physical address 0x%08x (0x%08x%08x%08x%08x)\n", addr, data.u32[3], data.u32[2], data.u32[1], data.u32[0]);
}
