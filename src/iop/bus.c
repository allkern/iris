#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "bus.h"
#include "bus_decl.h"

struct iop_bus* iop_bus_create(void) {
    return malloc(sizeof(struct iop_bus));
}

void iop_bus_init(struct iop_bus* bus, const char* bios_path) {
    memset(bus, 0, sizeof(struct iop_bus));
}

void iop_bus_init_bios(struct iop_bus* bus, struct ps2_bios* bios) {
    bus->bios = bios;
}

void iop_bus_init_iop_ram(struct iop_bus* bus, struct ps2_ram* iop_ram) {
    bus->iop_ram = iop_ram;
}

void iop_bus_init_iop_spr(struct iop_bus* bus, struct ps2_ram* iop_spr) {
    bus->iop_spr = iop_spr;
}

void iop_bus_init_sif(struct iop_bus* bus, struct ps2_sif* sif) {
    bus->sif = sif;
}

void iop_bus_init_dma(struct iop_bus* bus, struct ps2_iop_dma* dma) {
    bus->dma = dma;
}

void iop_bus_init_intc(struct iop_bus* bus, struct ps2_iop_intc* intc) {
    bus->intc = intc;
}

void iop_bus_init_timers(struct iop_bus* bus, struct ps2_iop_timers* timers) {
    bus->timers = timers;
}

void iop_bus_init_cdvd(struct iop_bus* bus, struct ps2_cdvd* cdvd) {
    bus->cdvd = cdvd;
}

void iop_bus_init_sio2(struct iop_bus* bus, struct ps2_sio2* sio2) {
    bus->sio2 = sio2;
}

void iop_bus_init_spu2(struct iop_bus* bus, struct ps2_spu2* spu2) {
    bus->spu2 = spu2;
}

void iop_bus_init_usb(struct iop_bus* bus, struct ps2_usb* usb) {
    bus->usb = usb;
}

void iop_bus_init_fw(struct iop_bus* bus, struct ps2_fw* fw) {
    bus->fw = fw;
}

void iop_bus_destroy(struct iop_bus* bus) {
    free(bus);
}

#define MAP_MEM_READ(b, l, u, d, n) \
    if ((addr >= l) && (addr <= u)) return ps2_ ## d ## _read ## b (bus->n, addr - l);

#define MAP_REG_READ(b, l, u, d, n) \
    if ((addr >= l) && (addr <= u)) return ps2_ ## d ## _read ## b (bus->n, addr);

#define MAP_MEM_WRITE(b, l, u, d, n) \
    if ((addr >= l) && (addr <= u)) { ps2_ ## d ## _write ## b (bus->n, addr - l, data); return; }

#define MAP_REG_WRITE(b, l, u, d, n) \
    if ((addr >= l) && (addr <= u)) { ps2_ ## d ## _write ## b (bus->n, addr, data); return; }

uint32_t iop_bus_read8(void* udata, uint32_t addr) {
    struct iop_bus* bus = (struct iop_bus*)udata;

    MAP_MEM_READ(8, 0x00000000, 0x001FFFFF, ram, iop_ram);
    MAP_MEM_READ(8, 0x1F800000, 0x1F8003FF, ram, iop_spr);
    MAP_REG_READ(8, 0x1F402004, 0x1F402018, cdvd, cdvd);
    MAP_REG_READ(8, 0x1F808200, 0x1F808280, sio2, sio2);
    MAP_MEM_READ(8, 0x1FC00000, 0x1FFFFFFF, bios, bios);

    // printf("iop_bus: Unhandled 8-bit read from physical address 0x%08x\n", addr);

    return 0;
}

uint32_t iop_bus_read16(void* udata, uint32_t addr) {
    struct iop_bus* bus = (struct iop_bus*)udata;

    MAP_MEM_READ(16, 0x00000000, 0x001FFFFF, ram, iop_ram);
    MAP_MEM_READ(16, 0x1F800000, 0x1F8003FF, ram, iop_spr);
    MAP_REG_READ(32, 0x1F801100, 0x1F80112F, iop_timers, timers);
    MAP_REG_READ(32, 0x1F801480, 0x1F8014AF, iop_timers, timers);
    MAP_REG_READ(16, 0x1F801080, 0x1F8010EF, iop_dma, dma);
    MAP_REG_READ(16, 0x1F801500, 0x1F80155F, iop_dma, dma);
    MAP_REG_READ(16, 0x1F801570, 0x1F80157F, iop_dma, dma);
    MAP_REG_READ(16, 0x1F8010F0, 0x1F8010F8, iop_dma, dma);
    MAP_REG_READ(16, 0x1F900000, 0x1F9007FF, spu2, spu2);
    MAP_MEM_READ(16, 0x1FC00000, 0x1FFFFFFF, bios, bios);

    // if (addr == 0x1f9001b0) {
    //     return 0xffff;
    // }
    // Stub SPU2 status reg?
    // if ((addr & 0xfff0000f) == 0x1f90000a) {
    //     return 0xffff;
    // }

    // printf("iop_bus: Unhandled 16-bit read from physical address 0x%08x\n", addr);

    return 0;
}

uint32_t iop_bus_read32(void* udata, uint32_t addr) {
    struct iop_bus* bus = (struct iop_bus*)udata;

    MAP_MEM_READ(32, 0x00000000, 0x001FFFFF, ram, iop_ram);
    MAP_MEM_READ(32, 0x1F800000, 0x1F8003FF, ram, iop_spr);
    MAP_REG_READ(32, 0x1D000000, 0x1D00006F, sif, sif);
    MAP_REG_READ(32, 0x1F801070, 0x1F80107B, iop_intc, intc);
    MAP_REG_READ(32, 0x1F801080, 0x1F8010EF, iop_dma, dma);
    MAP_REG_READ(32, 0x1F801500, 0x1F80155F, iop_dma, dma);
    MAP_REG_READ(32, 0x1F801570, 0x1F80157F, iop_dma, dma);
    MAP_REG_READ(32, 0x1F8010F0, 0x1F8010F8, iop_dma, dma);
    MAP_REG_READ(32, 0x1F801100, 0x1F80112F, iop_timers, timers);
    MAP_REG_READ(32, 0x1F801480, 0x1F8014AF, iop_timers, timers);
    MAP_REG_READ(32, 0x1F808200, 0x1F808280, sio2, sio2);
    MAP_MEM_READ(32, 0x1FC00000, 0x1FFFFFFF, bios, bios);
    MAP_REG_READ(32, 0x1F801600, 0x1F8016FF, usb, usb);
    MAP_REG_READ(32, 0x1F808400, 0x1F80854F, fw, fw);

    if (addr == 0x1f801450) return 0;
    if (addr == 0x1f801414) return 1;

    // if (addr == 0x1f801560) return 1;
    if ((addr & 0xff000000) == 0x1e000000) return 0;
    // Bloody Roar 4 Wrong IOP CDVD DMA
    // if ((addr & 0xff000000) == 0x0c000000) { *(uint8_t*)0 = 0; }

    // printf("iop_bus: Unhandled 32-bit read from physical address 0x%08x\n", addr);

    return 0;
}

void iop_bus_write8(void* udata, uint32_t addr, uint32_t data) {
    struct iop_bus* bus = (struct iop_bus*)udata;

    MAP_MEM_WRITE(8, 0x00000000, 0x001FFFFF, ram, iop_ram);
    MAP_MEM_WRITE(8, 0x1F800000, 0x1F8003FF, ram, iop_spr);
    MAP_REG_WRITE(8, 0x1F402004, 0x1F402018, cdvd, cdvd);
    MAP_REG_WRITE(32, 0x1F801070, 0x1F80107B, iop_intc, intc);
    MAP_REG_WRITE(32, 0x1F801080, 0x1F8010EF, iop_dma, dma);
    MAP_REG_WRITE(32, 0x1F801500, 0x1F80155F, iop_dma, dma);
    MAP_REG_WRITE(32, 0x1F801570, 0x1F80157F, iop_dma, dma);
    MAP_REG_WRITE(32, 0x1F8010F0, 0x1F8010F8, iop_dma, dma);
    MAP_REG_WRITE(8, 0x1F808200, 0x1F808280, sio2, sio2);
    MAP_MEM_WRITE(8, 0x1FC00000, 0x1FFFFFFF, bios, bios);

    // printf("iop_bus: Unhandled 8-bit write to physical address 0x%08x (0x%02x)\n", addr, data);
}

void iop_bus_write16(void* udata, uint32_t addr, uint32_t data) {
    struct iop_bus* bus = (struct iop_bus*)udata;

    MAP_MEM_WRITE(16, 0x00000000, 0x001FFFFF, ram, iop_ram);
    MAP_MEM_WRITE(16, 0x1F800000, 0x1F8003FF, ram, iop_spr);
    MAP_REG_WRITE(32, 0x1F801100, 0x1F80112F, iop_timers, timers);
    MAP_REG_WRITE(32, 0x1F801480, 0x1F8014AF, iop_timers, timers);
    MAP_REG_WRITE(32, 0x1F801070, 0x1F80107B, iop_intc, intc);
    MAP_REG_WRITE(16, 0x1F801080, 0x1F8010EF, iop_dma, dma);
    MAP_REG_WRITE(16, 0x1F801500, 0x1F80155F, iop_dma, dma);
    MAP_REG_WRITE(16, 0x1F801570, 0x1F80157F, iop_dma, dma);
    MAP_REG_WRITE(16, 0x1F8010F0, 0x1F8010F8, iop_dma, dma);
    MAP_REG_WRITE(16, 0x1F900000, 0x1F9007FF, spu2, spu2);
    MAP_MEM_WRITE(16, 0x1FC00000, 0x1FFFFFFF, bios, bios);

    // printf("iop_bus: Unhandled 16-bit write to physical address 0x%08x (0x%04x)\n", addr, data);
}

void iop_bus_write32(void* udata, uint32_t addr, uint32_t data) {
    struct iop_bus* bus = (struct iop_bus*)udata;

    MAP_MEM_WRITE(32, 0x00000000, 0x001FFFFF, ram, iop_ram);
    MAP_MEM_WRITE(32, 0x1F800000, 0x1F8003FF, ram, iop_spr);
    MAP_REG_WRITE(32, 0x1D000000, 0x1D00006F, sif, sif);
    MAP_REG_WRITE(32, 0x1F801070, 0x1F80107B, iop_intc, intc);
    MAP_REG_WRITE(32, 0x1F801080, 0x1F8010EF, iop_dma, dma);
    MAP_REG_WRITE(32, 0x1F801500, 0x1F80155F, iop_dma, dma);
    MAP_REG_WRITE(32, 0x1F801570, 0x1F80157F, iop_dma, dma);
    MAP_REG_WRITE(32, 0x1F8010F0, 0x1F8010F8, iop_dma, dma);
    MAP_REG_WRITE(32, 0x1F801100, 0x1F80112F, iop_timers, timers);
    MAP_REG_WRITE(32, 0x1F801480, 0x1F8014AF, iop_timers, timers);
    MAP_REG_WRITE(32, 0x1F808200, 0x1F808280, sio2, sio2);
    MAP_MEM_WRITE(32, 0x1FC00000, 0x1FFFFFFF, bios, bios);
    MAP_REG_WRITE(32, 0x1F801600, 0x1F8016FF, usb, usb);
    MAP_REG_WRITE(32, 0x1F808400, 0x1F80854F, fw, fw);

    if (addr == 0x1f801450) return;

    // printf("iop_bus: Unhandled 32-bit write to physical address 0x%08x (0x%08x)\n", addr, data);
}

#undef MAP_READ
#undef MAP_WRITE