#include "link.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

#define printf(fmt, ...)(0)

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

void s14x_link_init(struct s14x_link* link, struct ps2_iop_intc* intc, struct sched_state* sched) {
    memset(link, 0, sizeof(struct s14x_link));

    // ARCNET CORE interrupt (possibly sent after INIMODE was set?)
    // TA bit set
    link->stsl = 1;

    // RECON bit set
    link->comr0 = 4;

    link->intc = intc;
    link->sched = sched;
}

void link_send_irq(struct s14x_link* link, uint16_t irq) {
    link->stsl |= irq & 0xff;
    link->stsh |= (irq >> 8) & 0xff;

    ps2_iop_intc_irq(link->intc, IOP_INTC_DEV9);
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

            // printf("s14x_link: Read RAM[%04x] = %02x\n", addr, link->ram[addr]);

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

void s14x_link_register_node(struct s14x_link* link, int node, link_packet_handler handler, void* udata) {
    link->nodes[node].handler = handler;
    link->nodes[node].udata = udata;
}

// Note: pacmanap reverse engineering
//       mynode = 1, maxnodes = 2
//       Main board is at node 1
//       I/O board is at node 2

//       pacmanbr
//       node = 1, maxnodes = 3
//       Main board is at node 1
//       I/O board is at node 2
//       A.I./Standalone board is at node 3

void link_recv_reply(void* udata, int overshoot) {
    struct s14x_link* link = (struct s14x_link*)udata;

    struct link_packet* tx = (struct link_packet*)&link->ram[0x40];
    struct link_packet* rx = (struct link_packet*)&link->ram[tx->dst_node * 0x40];

    memset(rx, 0, sizeof(struct link_packet));

    // Set TA and TMA bits (transmitter available)
    link->stsl |= S14X_LINK_STSL_TA;
    link->comr0 |= S14X_LINK_COMR0_R_TA | S14X_LINK_COMR0_R_TMA;

    struct link_node* node = &link->nodes[tx->dst_node];

    if (!node->handler) {
        fprintf(stdout, "s14x_link: Packet sent to disconnected node %d (cmd=%02x cp=%02x)\n",
            tx->dst_node,
            tx->cmd,
            tx->cp
        );

        return;
    }

    // Set packet pending flag
    link->rxfll = 1 << tx->dst_node;

    // Get response from the requested node
    node->handler(node->udata, tx, rx);

    // Send DEV9 IRQ to IOP
    ps2_iop_intc_irq(link->intc, IOP_INTC_DEV9);
}

void s14x_link_write(struct s14x_link* link, uint32_t addr, uint64_t data) {
    if (addr != S14X_LINK_WATCHDOG_FLAG) {
        printf(stdout, "s14x_link: Write %s (%08x) %08x\n", g_reg_names[addr], addr, data);
    }

    switch (addr) {
        case S14X_LINK_PAD00: link->pad00 = data; return;
        case S14X_LINK_COMR0: {
            link->comr0 &= ~(S14X_LINK_COMR0_R_RECON | S14X_LINK_COMR0_W_TA);
            link->comr0 |= data & (S14X_LINK_COMR0_R_RECON | S14X_LINK_COMR0_W_TA);

            if (data & 0xf) {
                link_send_irq(link, S14X_LINK_IRQ_COM);
            }
        } return;
        case S14X_LINK_PAD02: link->pad02 = data; return;
        case S14X_LINK_COMR1: link->comr1 = data; return;
        case S14X_LINK_PAD04: link->pad04 = data; return;
        case S14X_LINK_COMR2: {
            link->comr2 = data;
            link->ramadr = (link->ramadr & 0x3f) | ((data & 0xf) << 6);
        } return;
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
        case S14X_LINK_STSH: {
            link->stsh &= 0x30;
            link->stsh |= data & 0xcf;
        } return;
        case S14X_LINK_STSL: {
            link->stsl &= 0x09;
            link->stsl |= data & 0xf6;
        } return;
        case S14X_LINK_MSKH: {
            link->mskh = data;
        } return;
        case S14X_LINK_MSKL: {
            link->mskl = data;
        } return;
        case S14X_LINK_PAD16: link->pad16 = data; return;
        case S14X_LINK_ECCMD: { 
            link->eccmd = data;

            switch (link->eccmd) {
                case 0x03: {
                    struct sched_event event;

                    event.callback = link_recv_reply;
                    event.udata = link;
                    event.cycles = 10000;
                    event.name = "Link reply";

                    link->stsl &= ~S14X_LINK_STSL_TA;
                    link->comr0 &= ~(S14X_LINK_COMR0_R_TA | S14X_LINK_COMR0_R_TMA);

                    sched_schedule(link->sched, event);
                } break;

                case 0x16: {
                    // Clear RECON bit
                    link->comr0 &= ~S14X_LINK_COMR0_W_RECON;
                } break;

                default: {
                    printf("s14x_link: Unhandled EC command %02x\n", link->eccmd);
                } break;
            }
        } return;
        case S14X_LINK_MRSID: link->mrsid = data; return;
        case S14X_LINK_RSID: link->rsid = data; return;
        case S14X_LINK_PAD1A: link->pad1a = data; return;
        case S14X_LINK_SSID: link->ssid = data; return;

        // Writing clears receive flags
        case S14X_LINK_RXFHH: link->rxfhh &= ~data; return;
        case S14X_LINK_RXFHL: link->rxfhl &= ~data; return;
        case S14X_LINK_RXFLH: link->rxflh &= ~data; return;
        case S14X_LINK_RXFLL: link->rxfll &= ~data; return;
        case S14X_LINK_PAD20: link->pad20 = data; return;
        case S14X_LINK_CMID: link->cmid = data; return;
        case S14X_LINK_MODEH: {
            // Software reset
            if (link->modeh & S14X_LINK_MODEH_INIMODE != data & S14X_LINK_MODEH_INIMODE) {
                link->stsl = 0;
                link->stsh = 0;
                link->mskl = 0;
                link->mskh = 0;
                link->comr0 = 0;
                link->comr1 = 0;
            }

            link->modeh = data;
        } break;
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