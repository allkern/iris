#include "link.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

const char* g_reg_names[] = {
    "S14X_LINK_PAD00",
    "S14X_LINK_COMR0",
    "S14X_LINK_PAD02",
    "S14X_LINK_COMR1",
    "S14X_LINK_PAD04",
    "S14X_LINK_COMR2",
    "S14X_LINK_PAD06",
    "S14X_LINK_COMR3",
    "S14X_LINK_PAD08",
    "S14X_LINK_COMR4",
    "S14X_LINK_PAD0A",
    "S14X_LINK_COMR5",
    "S14X_LINK_PAD0C",
    "S14X_LINK_COMR6",
    "S14X_LINK_PAD0E",
    "S14X_LINK_COMR7",
    "S14X_LINK_NSTH",
    "S14X_LINK_NSTL",
    "S14X_LINK_STSH",
    "S14X_LINK_STSL",
    "S14X_LINK_MSKH",
    "S14X_LINK_MSKL",
    "S14X_LINK_PAD16",
    "S14X_LINK_ECCMD",
    "S14X_LINK_MRSID",
    "S14X_LINK_RSID",
    "S14X_LINK_PAD1A",
    "S14X_LINK_SSID",
    "S14X_LINK_RXFHH",
    "S14X_LINK_RXFHL",
    "S14X_LINK_RXFLH",
    "S14X_LINK_RXFLL",
    "S14X_LINK_PAD20",
    "S14X_LINK_CMID",
    "S14X_LINK_MODEH",
    "S14X_LINK_MODEL",
    "S14X_LINK_CARRYH",
    "S14X_LINK_CARRYL",
    "S14X_LINK_RXMHH",
    "S14X_LINK_RXMHL",
    "S14X_LINK_RXMLH",
    "S14X_LINK_RXMLL",
    "S14X_LINK_PAD2A",
    "S14X_LINK_MAXID",
    "S14X_LINK_PAD2C",
    "S14X_LINK_NID",
    "S14X_LINK_PAD2E",
    "S14X_LINK_PS",
    "S14X_LINK_PAD30",
    "S14X_LINK_CKP",
    "S14X_LINK_NSTDIFH",
    "S14X_LINK_NSTDIFL",
    "S14X_LINK_WATCHDOG_FLAG"
};

struct s14x_link* s14x_link_create(void) {
    return malloc(sizeof(struct s14x_link));
}

void s14x_link_init(struct s14x_link* link) {
    memset(link, 0, sizeof(struct s14x_link));
}

uint64_t s14x_link_read(struct s14x_link* link, uint32_t addr) {
    uint32_t r = 0;

    switch (addr) {
        case S14X_LINK_PAD00: r = link->pad00; break;
        case S14X_LINK_COMR0: r = link->comr0; break;
        case S14X_LINK_PAD02: r = link->pad02; break;
        case S14X_LINK_COMR1: r = link->comr1; break;
        case S14X_LINK_PAD04: r = link->pad04; break;
        case S14X_LINK_COMR2: r = link->comr2; break;
        case S14X_LINK_PAD06: r = link->pad06; break;
        case S14X_LINK_COMR3: r = link->comr3; break;
        case S14X_LINK_PAD08: r = link->pad08; break;
        case S14X_LINK_COMR4: {
            uint32_t addr = link->ramadr;

            if (link->comr2 & S14X_LINK_COMR2_AUTOINC) {
                if (link->comr2 & S14X_LINK_COMR2_WRAPAR) {
                    link->ramadr = (link->ramadr & 0x3c0) | ((link->ramadr + 1) & 0x3f);
                } else {
                    link->ramadr = (link->ramadr + 1) & 0x3ff;
                }
            }

            printf("s14x_link: Read RAM[%04x] = %02x\n", addr, link->ram[addr]);

            r = link->ram[addr];
        } break;
        case S14X_LINK_PAD0A: r = link->pad0a; break;
        case S14X_LINK_COMR5: r = link->comr5; break;
        case S14X_LINK_PAD0C: r = link->pad0c; break;
        case S14X_LINK_COMR6: r = link->comr6; break;
        case S14X_LINK_PAD0E: r = link->pad0e; break;
        case S14X_LINK_COMR7: r = link->comr7; break;
        case S14X_LINK_NSTH: r = link->nsth; break;
        case S14X_LINK_NSTL: r = link->nstl; break;
        case S14X_LINK_STSH: r = link->stsh; break;
        case S14X_LINK_STSL: r = link->stsl; break;
        case S14X_LINK_MSKH: r = link->mskh; break;
        case S14X_LINK_MSKL: r = link->mskl; break;
        case S14X_LINK_PAD16: r = link->pad16; break;
        case S14X_LINK_ECCMD: r = link->eccmd; break;
        case S14X_LINK_MRSID: r = link->mrsid; break;
        case S14X_LINK_RSID: r = link->rsid; break;
        case S14X_LINK_PAD1A: r = link->pad1a; break;
        case S14X_LINK_SSID: r = link->ssid; break;
        case S14X_LINK_RXFHH: r = link->rxfhh; break;
        case S14X_LINK_RXFHL: r = link->rxfhl; break;
        case S14X_LINK_RXFLH: r = link->rxflh; break;
        case S14X_LINK_RXFLL: r = link->rxfll; break;
        case S14X_LINK_PAD20: r = link->pad20; break;
        case S14X_LINK_CMID: r = link->cmid; break;
        case S14X_LINK_MODEH: r = link->modeh; break;
        case S14X_LINK_MODEL: r = link->model; break;
        case S14X_LINK_CARRYH: r = link->carryh; break;
        case S14X_LINK_CARRYL: r = link->carryl; break;
        case S14X_LINK_RXMHH: r = link->rxmhh; break;
        case S14X_LINK_RXMHL: r = link->rxmhl; break;
        case S14X_LINK_RXMLH: r = link->rxmlh; break;
        case S14X_LINK_RXMLL: r = link->rxmll; break;
        case S14X_LINK_PAD2A: r = link->pad2a; break;
        case S14X_LINK_MAXID: r = link->maxid; break;
        case S14X_LINK_PAD2C: r = link->pad2c; break;
        case S14X_LINK_NID: r = link->nid; break;
        case S14X_LINK_PAD2E: r = link->pad2e; break;
        case S14X_LINK_PS: r = link->ps; break;
        case S14X_LINK_PAD30: r = link->pad30; break;
        case S14X_LINK_CKP: r = link->ckp; break;
        case S14X_LINK_NSTDIFH: r = link->nstdifh; break;
        case S14X_LINK_NSTDIFL: r = link->nstdifl; break;
        case S14X_LINK_WATCHDOG_FLAG: r = link->watchdog_flag; break;
    }

    if (addr != S14X_LINK_WATCHDOG_FLAG) {
        printf("s14x_link: Read %s (%08x) %08x\n", g_reg_names[addr], addr, r);
    }

    return r;
}

void s14x_link_write(struct s14x_link* link, uint32_t addr, uint64_t data) {
    if (addr != S14X_LINK_WATCHDOG_FLAG) {
        printf("s14x_link: Write %s (%08x) %08x\n", g_reg_names[addr], addr, data);
    }

    switch (addr) {
        case S14X_LINK_PAD00: link->pad00 = data; return;
        case S14X_LINK_COMR0: link->comr0 = data; return;
        case S14X_LINK_PAD02: link->pad02 = data; return;
        case S14X_LINK_COMR1: link->comr1 = data; return;
        case S14X_LINK_PAD04: link->pad04 = data; return;
        case S14X_LINK_COMR2: {
            link->comr2 = data;
            link->ramadr = (link->ramadr & 0x3f) | ((data & 0xf) << 6);
        }
        case S14X_LINK_PAD06: link->pad06 = data; return;
        case S14X_LINK_COMR3: {
            link->comr3 = data;
            link->ramadr = (link->ramadr & 0x3c0) | (data & 0x3f);
        } return;
        case S14X_LINK_PAD08: link->pad08 = data; return;
        case S14X_LINK_COMR4: {
            uint32_t addr = link->ramadr;

            if (link->comr2 & S14X_LINK_COMR2_AUTOINC) {
                if (link->comr2 & S14X_LINK_COMR2_WRAPAR) {
                    link->ramadr = (link->ramadr + 1) & 0x3ff;
                } else {
                    link->ramadr = (link->ramadr & 0x3c0) | ((link->ramadr + 1) & 0x3f);
                }
            }

            // printf("s14x_link: Write RAM[%04x] = %02x\n", addr, data);

            link->ram[addr] = data;
        } return;
        case S14X_LINK_PAD0A: link->pad0a = data; return;
        case S14X_LINK_COMR5: link->comr5 = data; return;
        case S14X_LINK_PAD0C: link->pad0c = data; return;
        case S14X_LINK_COMR6: link->comr6 = data; return;
        case S14X_LINK_PAD0E: link->pad0e = data; return;
        case S14X_LINK_COMR7: link->comr7 = data; return;
        case S14X_LINK_NSTH: link->nsth = data; return;
        case S14X_LINK_NSTL: link->nstl = data; return;
        case S14X_LINK_STSH: link->stsh = data; return;
        case S14X_LINK_STSL: link->stsl = data; return;
        case S14X_LINK_MSKH: link->mskh = data; return;
        case S14X_LINK_MSKL: link->mskl = data; return;
        case S14X_LINK_PAD16: link->pad16 = data; return;
        case S14X_LINK_ECCMD: link->eccmd = data; return;
        case S14X_LINK_MRSID: link->mrsid = data; return;
        case S14X_LINK_RSID: link->rsid = data; return;
        case S14X_LINK_PAD1A: link->pad1a = data; return;
        case S14X_LINK_SSID: link->ssid = data; return;
        case S14X_LINK_RXFHH: link->rxfhh = data; return;
        case S14X_LINK_RXFHL: link->rxfhl = data; return;
        case S14X_LINK_RXFLH: link->rxflh = data; return;
        case S14X_LINK_RXFLL: link->rxfll = data; return;
        case S14X_LINK_PAD20: link->pad20 = data; return;
        case S14X_LINK_CMID: link->cmid = data; return;
        case S14X_LINK_MODEH: link->modeh = data; return;
        case S14X_LINK_MODEL: link->model = data; return;
        case S14X_LINK_CARRYH: link->carryh = data; return;
        case S14X_LINK_CARRYL: link->carryl = data; return;
        case S14X_LINK_RXMHH: link->rxmhh = data; return;
        case S14X_LINK_RXMHL: link->rxmhl = data; return;
        case S14X_LINK_RXMLH: link->rxmlh = data; return;
        case S14X_LINK_RXMLL: link->rxmll = data; return;
        case S14X_LINK_PAD2A: link->pad2a = data; return;
        case S14X_LINK_MAXID: link->maxid = data; return;
        case S14X_LINK_PAD2C: link->pad2c = data; return;
        case S14X_LINK_NID: link->nid = data; return;
        case S14X_LINK_PAD2E: link->pad2e = data; return;
        case S14X_LINK_PS: link->ps = data; return;
        case S14X_LINK_PAD30: link->pad30 = data; return;
        case S14X_LINK_CKP: link->ckp = data; return;
        case S14X_LINK_NSTDIFH: link->nstdifh = data; return;
        case S14X_LINK_NSTDIFL: link->nstdifl = data; return;
        case S14X_LINK_WATCHDOG_FLAG: link->watchdog_flag = data; return;
    }
}

void s14x_link_destroy(struct s14x_link* link) {
    free(link);
}