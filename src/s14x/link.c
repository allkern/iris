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
    if (addr != S14X_LINK_WATCHDOG_FLAG) {
        printf("s14x_link: Read %s (%08x)\n", g_reg_names[addr], addr);
    }

    switch (addr) {
        case S14X_LINK_PAD00: return link->pad00;
        case S14X_LINK_COMR0: return link->comr0;
        case S14X_LINK_PAD02: return link->pad02;
        case S14X_LINK_COMR1: return link->comr1;
        case S14X_LINK_PAD04: return link->pad04;
        case S14X_LINK_COMR2: return link->comr2;
        case S14X_LINK_PAD06: return link->pad06;
        case S14X_LINK_COMR3: return link->comr3;
        case S14X_LINK_PAD08: return link->pad08;
        case S14X_LINK_COMR4: return link->comr4;
        case S14X_LINK_PAD0A: return link->pad0a;
        case S14X_LINK_COMR5: return link->comr5;
        case S14X_LINK_PAD0C: return link->pad0c;
        case S14X_LINK_COMR6: return link->comr6;
        case S14X_LINK_PAD0E: return link->pad0e;
        case S14X_LINK_COMR7: return link->comr7;
        case S14X_LINK_NSTH: return link->nsth;
        case S14X_LINK_NSTL: return link->nstl;
        case S14X_LINK_STSH: return link->stsh;
        case S14X_LINK_STSL: return link->stsl;
        case S14X_LINK_MSKH: return link->mskh;
        case S14X_LINK_MSKL: return link->mskl;
        case S14X_LINK_PAD16: return link->pad16;
        case S14X_LINK_ECCMD: return link->eccmd;
        case S14X_LINK_MRSID: return link->mrsid;
        case S14X_LINK_RSID: return link->rsid;
        case S14X_LINK_PAD1A: return link->pad1a;
        case S14X_LINK_SSID: return link->ssid;
        case S14X_LINK_RXFHH: return link->rxfhh;
        case S14X_LINK_RXFHL: return link->rxfhl;
        case S14X_LINK_RXFLH: return link->rxflh;
        case S14X_LINK_RXFLL: return link->rxfll;
        case S14X_LINK_PAD20: return link->pad20;
        case S14X_LINK_CMID: return link->cmid;
        case S14X_LINK_MODEH: return link->modeh;
        case S14X_LINK_MODEL: return link->model;
        case S14X_LINK_CARRYH: return link->carryh;
        case S14X_LINK_CARRYL: return link->carryl;
        case S14X_LINK_RXMHH: return link->rxmhh;
        case S14X_LINK_RXMHL: return link->rxmhl;
        case S14X_LINK_RXMLH: return link->rxmlh;
        case S14X_LINK_RXMLL: return link->rxmll;
        case S14X_LINK_PAD2A: return link->pad2a;
        case S14X_LINK_MAXID: return link->maxid;
        case S14X_LINK_PAD2C: return link->pad2c;
        case S14X_LINK_NID: return link->nid;
        case S14X_LINK_PAD2E: return link->pad2e;
        case S14X_LINK_PS: return link->ps;
        case S14X_LINK_PAD30: return link->pad30;
        case S14X_LINK_CKP: return link->ckp;
        case S14X_LINK_NSTDIFH: return link->nstdifh;
        case S14X_LINK_NSTDIFL: return link->nstdifl;
        case S14X_LINK_WATCHDOG_FLAG: return link->watchdog_flag;
    }

    return 0;
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
        case S14X_LINK_COMR2: link->comr2 = data; return;
        case S14X_LINK_PAD06: link->pad06 = data; return;
        case S14X_LINK_COMR3: link->comr3 = data; return;
        case S14X_LINK_PAD08: link->pad08 = data; return;
        case S14X_LINK_COMR4: link->comr4 = data; return;
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
        case S14X_LINK_ECCMD: link->eccmd = data; exit(1); return;
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