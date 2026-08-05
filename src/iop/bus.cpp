#include <new>

#include "dma.hpp"
#include "bus.hpp"
#include "iop/cdvd.hpp"
#include "bus_decl.hpp"

namespace iris::iop::bus {

Bus* create(logger::Logger* logger) {
    Bus* bus = new Bus();

    bus->logger = logger;
    bus->logger_id = logger::register_source(logger, "iop_bus");

    return bus;
}

#define RAM_MAX_SIZE 0x1000000

void init_fastmem(Bus* bus, int ram_size) {
    memset(bus->fastmem_r_table, 0, sizeof(bus->fastmem_r_table));
    memset(bus->fastmem_w_table, 0, sizeof(bus->fastmem_w_table));

    // BIOS
    for (int i = 0; i < 0x200; i++) {
        bus->fastmem_r_table[i+0xfe00] = bus->bios->buf + (i * 0x2000);
    }

    // IOP RAM
    int mask = ram_size - 1;

    for (int i = 0; i < (RAM_MAX_SIZE / 0x2000); i++) {
        bus->fastmem_r_table[i+0x0000] = bus->iop_ram->buf + ((i * 0x2000) & mask);
        bus->fastmem_w_table[i+0x0000] = bus->iop_ram->buf + ((i * 0x2000) & mask);
    }
}

void set_usb_disabled(Bus* bus, int disabled) {
    bus->disable_usb = disabled;
}

int is_usb_disabled(Bus* bus) {
    return bus->disable_usb;
}

void destroy(Bus* bus) {
    delete bus;
}

#define MAP_MEM_READ_NS(b, l, u, d, n)     if ((addr >= l) && (addr <= u)) return d::read ## b (bus->n, addr - l);

#define MAP_REG_READ_NS(b, l, u, d, n)     if ((addr >= l) && (addr <= u)) return d::read ## b (bus->n, addr);

#define MAP_MEM_WRITE_NS(b, l, u, d, n)     if ((addr >= l) && (addr <= u)) { d::write ## b (bus->n, addr - l, data); return; }

#define MAP_REG_WRITE_NS(b, l, u, d, n)     if ((addr >= l) && (addr <= u)) { d::write ## b (bus->n, addr, data); return; }

#define MAP_MEM_READ(b, l, u, d, n) \
    if ((addr >= l) && (addr <= u)) return ps2_ ## d ## _read ## b (bus->n, addr - l);

#define MAP_REG_READ(b, l, u, d, n) \
    if ((addr >= l) && (addr <= u)) return ps2_ ## d ## _read ## b (bus->n, addr);

#define MAP_MEM_WRITE(b, l, u, d, n) \
    if ((addr >= l) && (addr <= u)) { ps2_ ## d ## _write ## b (bus->n, addr - l, data); return; }

#define MAP_REG_WRITE(b, l, u, d, n) \
    if ((addr >= l) && (addr <= u)) { ps2_ ## d ## _write ## b (bus->n, addr, data); return; }

#define MAP_REG_READ_CHK_NS(b, l, u, d, n) \
    if (bus->n && (addr >= l) && (addr <= u)) return d::read ## b (bus->n, addr);

#define MAP_REG_WRITE_CHK_NS(b, l, u, d, n) \
    if (bus->n && (addr >= l) && (addr <= u)) { d::write ## b (bus->n, addr, data); return; }

#define MAP_MEM_READ_CHK_NS(b, l, u, d, n) \
    if (bus->n && (addr >= l) && (addr <= u)) return d::read ## b (bus->n, addr - l);

#define MAP_MEM_WRITE_CHK_NS(b, l, u, d, n) \
    if (bus->n && (addr >= l) && (addr <= u)) { d::write ## b (bus->n, addr - l, data); return; }

#define MAP_REG_READ_CHK(b, l, u, d, n) \
    if (bus->n && (addr >= l) && (addr <= u)) return d ## _read ## b (bus->n, addr);

#define MAP_REG_WRITE_CHK(b, l, u, d, n) \
    if (bus->n && (addr >= l) && (addr <= u)) { d ## _write ## b (bus->n, addr, data); return; }

#define MAP_MEM_READ_CHK(b, l, u, d, n) \
    if (bus->n && (addr >= l) && (addr <= u)) return d ## _read ## b (bus->n, addr - l);

#define MAP_MEM_WRITE_CHK(b, l, u, d, n) \
    if (bus->n && (addr >= l) && (addr <= u)) { d ## _write ## b (bus->n, addr - l, data); return; }

uint32_t read8(void* udata, uint32_t addr) {
    Bus* bus = (Bus*)udata;

    void* ptr = bus->fastmem_r_table[(addr & 0x1fffffff) >> 13];

    if (ptr) return *((uint8_t*)(((uint8_t*)ptr) + (addr & 0x1fff)));

    // MAP_MEM_READ_NS(8, 0x00000000, 0x001FFFFF, ram, iop_ram);
    // MAP_MEM_READ_NS(8, 0x1FC00000, 0x1FFFFFFF, bios, bios);
    MAP_MEM_READ_NS(8, 0x1F800000, 0x1F8003FF, ram, iop_spr);
    MAP_REG_READ_NS(8, 0x1F801070, 0x1F80107B, iop::intc, intc);
    MAP_REG_READ_NS(8, 0x1F402004, 0x1F4020FF, cdvd, cdvd);
    MAP_REG_READ_NS(8, 0x1F808200, 0x1F808280, sio2, sio2);
    MAP_MEM_READ_NS(8, 0x1E000000, 0x1E3FFFFF, bios, rom1);
    MAP_MEM_READ_NS(8, 0x1E400000, 0x1E7FFFFF, bios, rom2);
    MAP_REG_READ_NS(8, 0x1F801460, 0x1F80147F, dev9, dev9);

    // System 147/148 mappings
    MAP_REG_READ_CHK_NS(8, 0x10000000, 0x1000000F, s14x::syscon, s14x_syscon);
    MAP_REG_READ_CHK_NS(8, 0x10800000, 0x108000FF, s14x::link, s14x_link);
    MAP_MEM_READ_CHK_NS(8, 0x10C00000, 0x10C07FFF, s14x::sram, s14x_sram);
    MAP_REG_READ_CHK_NS(8, 0x14000000, 0x1400000F, s14x::nand, s14x_nand);

    // System 147/148 syscon overlays retail SPEED
    MAP_REG_READ_NS(8, 0x10000000, 0x1000FFFF, speed, speed);

    switch (addr) {
        // Required for T10000 TOOL BIOS
        // Otherwise the IOP hangs during initialization if the RAM size
        // is 8 MB or hangs with a stack overflow after init if the RAM 
        // size is 16 MB
        case 0x1f803204: return 0x7c;
    }

    // iris_debug(bus, "Bus: Unhandled 8-bit read from physical address 0x{:08x}", addr);

    return 0;
}

uint32_t read16(void* udata, uint32_t addr) {
    Bus* bus = (Bus*)udata;

    void* ptr = bus->fastmem_r_table[(addr & 0x1fffffff) >> 13];

    if (ptr) return *((uint16_t*)(((uint8_t*)ptr) + (addr & 0x1fff)));

    // MAP_MEM_READ_NS(16, 0x1FC00000, 0x1FFFFFFF, bios, bios);
    // MAP_MEM_READ_NS(16, 0x00000000, 0x001FFFFF, ram, iop_ram);
    MAP_MEM_READ_NS(16, 0x1F800000, 0x1F8003FF, ram, iop_spr);
    MAP_REG_READ_NS(16, 0x1F801070, 0x1F80107B, iop::intc, intc);
    MAP_REG_READ_NS(32, 0x1F801100, 0x1F80112F, iop::timers, timers);
    MAP_REG_READ_NS(32, 0x1F801480, 0x1F8014AF, iop::timers, timers);
    MAP_REG_READ_NS(16, 0x1F801080, 0x1F8010EF, dma, dma);
    MAP_REG_READ_NS(16, 0x1F801500, 0x1F80155F, dma, dma);
    MAP_REG_READ_NS(16, 0x1F801570, 0x1F80157F, dma, dma);
    MAP_REG_READ_NS(16, 0x1F8010F0, 0x1F8010F8, dma, dma);
    MAP_REG_READ_NS(16, 0x1F900000, 0x1F9007FF, spu2, spu2);
    MAP_MEM_READ_NS(16, 0x1E000000, 0x1E3FFFFF, bios, rom1);
    MAP_MEM_READ_NS(16, 0x1E400000, 0x1E7FFFFF, bios, rom2);
    MAP_REG_READ_NS(16, 0x1F801460, 0x1F80147F, dev9, dev9);
    MAP_REG_READ_NS(16, 0x10000000, 0x1000FFFF, speed, speed);

    // System 147/148 mappings
    MAP_MEM_READ_CHK_NS(16, 0x10C00000, 0x10C07FFF, s14x::sram, s14x_sram);

    // System 246/256 mappings
    MAP_REG_READ_CHK_NS(16, 0x12400000, 0x12407FFF, s2x6::acjv, s2x6_acjv);
    MAP_REG_READ_CHK_NS(16, 0x16000000, 0x1616FFFF, s2x6::acata, s2x6_acata);

    // PSX DESR
    if (addr == 0x1000480c) return 0xffff;

    // 0x20 - PCMCIA (CXD9566)
    // 0x30 - Expansion bay
    if (addr == 0x1f80146e) { return 0x30; }

    // SPEED rev3 (Capabilities)
    // bit 0 - SMAP
    // bit 1 - ATA
    // bit 3 - UART
    // bit 4 - DVR
    // bit 5 - FLASH
    // if (addr == 0x10000004) { return 0x03; }
    // if (addr == 0x1000205c) { return 0xffff; }
    // if (addr == 0x1000205e) { return 0xffff; }

    if (addr == 0x1241c000) return 0xffff;

    // iris_debug(bus, "Bus: Unhandled 16-bit read from physical address 0x{:08x}", addr);

    return 0;
}

uint32_t read32(void* udata, uint32_t addr) {
    Bus* bus = (Bus*)udata;

    if (addr == 0xfffe0130) return 0xffffffff;

    void* ptr = bus->fastmem_r_table[(addr & 0x1fffffff) >> 13];

    if (ptr) return *((uint32_t*)(((uint8_t*)ptr) + (addr & 0x1fff)));

    // MAP_MEM_READ_NS(32, 0x00000000, 0x001FFFFF, ram, iop_ram);
    // MAP_MEM_READ_NS(32, 0x1FC00000, 0x1FFFFFFF, bios, bios);
    MAP_MEM_READ_NS(32, 0x1F800000, 0x1F8003FF, ram, iop_spr);
    MAP_REG_READ_NS(32, 0x1D000000, 0x1D00006F, sif, sif);
    MAP_REG_READ_NS(32, 0x1F801070, 0x1F80107B, iop::intc, intc);
    MAP_REG_READ_NS(32, 0x1F801080, 0x1F8010EF, dma, dma);
    MAP_REG_READ_NS(32, 0x1F801500, 0x1F80155F, dma, dma);
    MAP_REG_READ_NS(32, 0x1F801570, 0x1F80157F, dma, dma);
    MAP_REG_READ_NS(32, 0x1F8010F0, 0x1F8010F8, dma, dma);
    MAP_REG_READ_NS(32, 0x1F801100, 0x1F80112F, iop::timers, timers);
    MAP_REG_READ_NS(32, 0x1F801480, 0x1F8014AF, iop::timers, timers);
    MAP_REG_READ_NS(32, 0x1F808200, 0x1F808280, sio2, sio2);

    // USB is not present on System 147/148?
    if (!bus->disable_usb) {
        MAP_REG_READ_NS(32, 0x1F801600, 0x1F8016FF, usb, usb);
    }

    MAP_REG_READ_NS(32, 0x1F808400, 0x1F80854F, fw, fw);
    MAP_MEM_READ_NS(32, 0x1E000000, 0x1E3FFFFF, bios, rom1);
    MAP_MEM_READ_NS(32, 0x1E400000, 0x1E7FFFFF, bios, rom2);
    MAP_REG_READ_NS(32, 0x1F801460, 0x1F80147F, dev9, dev9);
    MAP_REG_READ_NS(32, 0x10000000, 0x1000FFFF, speed, speed);

    // System 147/148 mappings
    MAP_MEM_READ_CHK_NS(32, 0x12400000, 0x12407FFF, s2x6::acjv, s2x6_acjv);
    MAP_MEM_READ_CHK_NS(32, 0x10C00000, 0x10C07FFF, s14x::sram, s14x_sram);

    if (addr == 0x1f801450) return 0;
    if (addr == 0x1f801414) return 1;
    if (addr == 0x1f801560) return 0;
    if (addr == 0x1f801014) return 0;

    if ((addr & 0xff000000) == 0x1e000000) return 0;
    if (addr == 0xfffe0130) return 0xffffffff;

    // Bloody Roar 4 Wrong IOP CDVD DMA
    // if ((addr & 0xff000000) == 0x0c000000) { *(uint8_t*)0 = 0; }

    // iris_debug(bus, "Bus: Unhandled 32-bit read from physical address 0x{:08x}", addr);

    return 0;
}

void write8(void* udata, uint32_t addr, uint32_t data) {
    Bus* bus = (Bus*)udata;

    void* ptr = bus->fastmem_w_table[(addr & 0x1fffffff) >> 13];

    if (ptr) {
        *((uint8_t*)(((uint8_t*)ptr) + (addr & 0x1fff))) = data;

        return;
    }

    // MAP_MEM_WRITE_NS(8, 0x00000000, 0x001FFFFF, ram, iop_ram);
    // MAP_MEM_WRITE_NS(8, 0x1FC00000, 0x1FFFFFFF, bios, bios);
    MAP_MEM_WRITE_NS(8, 0x1F800000, 0x1F8003FF, ram, iop_spr);
    MAP_REG_WRITE_NS(8, 0x1F402004, 0x1F4020FF, cdvd, cdvd);
    MAP_REG_WRITE_NS(8, 0x1F801070, 0x1F80107B, iop::intc, intc);
    MAP_REG_WRITE_NS(32, 0x1F801080, 0x1F8010EF, dma, dma);
    MAP_REG_WRITE_NS(32, 0x1F801500, 0x1F80155F, dma, dma);
    MAP_REG_WRITE_NS(32, 0x1F801570, 0x1F80157F, dma, dma);
    MAP_REG_WRITE_NS(32, 0x1F8010F0, 0x1F8010F8, dma, dma);
    MAP_REG_WRITE_NS(8, 0x1F808200, 0x1F808280, sio2, sio2);
    MAP_REG_WRITE_NS(8, 0x1F801460, 0x1F80147F, dev9, dev9);

    // System 147/148 mappings
    MAP_REG_WRITE_CHK_NS(8, 0x10000000, 0x1000000F, s14x::syscon, s14x_syscon);
    MAP_REG_WRITE_CHK_NS(8, 0x10800000, 0x108000FF, s14x::link, s14x_link);
    MAP_MEM_WRITE_CHK_NS(8, 0x10C00000, 0x10C07FFF, s14x::sram, s14x_sram);
    MAP_REG_WRITE_CHK_NS(8, 0x14000000, 0x1400000F, s14x::nand, s14x_nand);

    // System 147/148 syscon overlays retail SPEED
    MAP_REG_WRITE_NS(8, 0x10000000, 0x1000FFFF, speed, speed);

    iris_debug(bus, "Bus: Unhandled 8-bit write to physical address 0x{:08x} (0x{:02x})", addr, data);
}

void write16(void* udata, uint32_t addr, uint32_t data) {
    Bus* bus = (Bus*)udata;

    void* ptr = bus->fastmem_w_table[(addr & 0x1fffffff) >> 13];

    if (ptr) {
        *((uint16_t*)(((uint8_t*)ptr) + (addr & 0x1fff))) = data;

        return;
    }

    // MAP_MEM_WRITE_NS(16, 0x00000000, 0x001FFFFF, ram, iop_ram);
    // MAP_MEM_WRITE_NS(16, 0x1FC00000, 0x1FFFFFFF, bios, bios);
    MAP_MEM_WRITE_NS(16, 0x1F800000, 0x1F8003FF, ram, iop_spr);
    MAP_REG_WRITE_NS(32, 0x1F801100, 0x1F80112F, iop::timers, timers);
    MAP_REG_WRITE_NS(32, 0x1F801480, 0x1F8014AF, iop::timers, timers);
    MAP_REG_WRITE_NS(16, 0x1F801070, 0x1F80107B, iop::intc, intc);
    MAP_REG_WRITE_NS(16, 0x1F801080, 0x1F8010EF, dma, dma);
    MAP_REG_WRITE_NS(16, 0x1F801500, 0x1F80155F, dma, dma);
    MAP_REG_WRITE_NS(16, 0x1F801570, 0x1F80157F, dma, dma);
    MAP_REG_WRITE_NS(16, 0x1F8010F0, 0x1F8010F8, dma, dma);
    MAP_REG_WRITE_NS(16, 0x1F900000, 0x1F9007FF, spu2, spu2);
    MAP_REG_WRITE_NS(16, 0x1F801460, 0x1F80147F, dev9, dev9);
    MAP_REG_WRITE_NS(16, 0x10000000, 0x1000FFFF, speed, speed);

    // System 147/148 mappings
    MAP_MEM_WRITE_CHK_NS(16, 0x10C00000, 0x10C07FFF, s14x::sram, s14x_sram);

    // System 246/256 mappings
    MAP_REG_WRITE_CHK_NS(16, 0x12400000, 0x12407FFF, s2x6::acjv, s2x6_acjv);
    MAP_REG_WRITE_CHK_NS(16, 0x16000000, 0x1616FFFF, s2x6::acata, s2x6_acata);

    // iris_debug(bus, "Bus: Unhandled 16-bit write to physical address 0x{:08x} (0x{:04x})", addr, data);
}

void write32(void* udata, uint32_t addr, uint32_t data) {
    Bus* bus = (Bus*)udata;

    void* ptr = bus->fastmem_w_table[(addr & 0x1fffffff) >> 13];

    if (ptr) {
        *((uint32_t*)(((uint8_t*)ptr) + (addr & 0x1fff))) = data;

        return;
    }

    // BIU config
    if (addr == 0xfffe0130) return;

    // MAP_MEM_WRITE_NS(32, 0x00000000, 0x001FFFFF, ram, iop_ram);
    // MAP_MEM_WRITE_NS(32, 0x1FC00000, 0x1FFFFFFF, bios, bios);
    MAP_MEM_WRITE_NS(32, 0x1F800000, 0x1F8003FF, ram, iop_spr);
    MAP_REG_WRITE_NS(32, 0x1D000000, 0x1D00006F, sif, sif);
    MAP_REG_WRITE_NS(32, 0x1F801450, 0x1F801453, sbus, sbus);
    MAP_REG_WRITE_NS(32, 0x1F801070, 0x1F80107B, iop::intc, intc);
    MAP_REG_WRITE_NS(32, 0x1F801080, 0x1F8010EF, dma, dma);
    MAP_REG_WRITE_NS(32, 0x1F801500, 0x1F80155F, dma, dma);
    MAP_REG_WRITE_NS(32, 0x1F801570, 0x1F80157F, dma, dma);
    MAP_REG_WRITE_NS(32, 0x1F8010F0, 0x1F8010F8, dma, dma);
    MAP_REG_WRITE_NS(32, 0x1F801100, 0x1F80112F, iop::timers, timers);
    MAP_REG_WRITE_NS(32, 0x1F801480, 0x1F8014AF, iop::timers, timers);
    MAP_REG_WRITE_NS(32, 0x1F808200, 0x1F808280, sio2, sio2);

    // USB is not present on System 147/148?
    if (!bus->disable_usb) {
        MAP_REG_WRITE_NS(32, 0x1F801600, 0x1F8016FF, usb, usb);
    }

    MAP_REG_WRITE_NS(32, 0x1F808400, 0x1F80854F, fw, fw);
    MAP_REG_WRITE_NS(32, 0x1F801460, 0x1F80147F, dev9, dev9);
    MAP_REG_WRITE_NS(32, 0x10000000, 0x1000FFFF, speed, speed);

    // System 147/148 mappings
    MAP_MEM_WRITE_CHK_NS(32, 0x12400000, 0x12407FFF, s2x6::acjv, s2x6_acjv);
    MAP_MEM_WRITE_CHK_NS(32, 0x10C00000, 0x10C07FFF, s14x::sram, s14x_sram);

    // iris_debug(bus, "Bus: Unhandled 32-bit write to physical address 0x{:08x} (0x{:08x})", addr, data);
}

#undef MAP_READ
#undef MAP_WRITE

}
