#include <new>
#include "link.hpp"

#include <cctype>

namespace iris::s14x::link {

const char* g_reg_names[] = {
    "PAD00",
    "COMR0",
    "PAD02",
    "COMR1",
    "PAD04",
    "COMR2",
    "PAD06",
    "COMR3",
    "PAD08",
    "COMR4",
    "PAD0A",
    "COMR5",
    "PAD0C",
    "COMR6",
    "PAD0E",
    "COMR7",
    "NSTH",
    "NSTL",
    "STSH",
    "STSL",
    "MSKH",
    "MSKL",
    "PAD16",
    "ECCMD",
    "MRSID",
    "RSID",
    "PAD1A",
    "SSID",
    "RXFHH",
    "RXFHL",
    "RXFLH",
    "RXFLL",
    "PAD20",
    "CMID",
    "MODEH",
    "MODEL",
    "CARRYH",
    "CARRYL",
    "RXMHH",
    "RXMHL",
    "RXMLH",
    "RXMLL",
    "PAD2A",
    "MAXID",
    "PAD2C",
    "NID",
    "PAD2E",
    "PS",
    "PAD30",
    "CKP",
    "NSTDIFH",
    "NSTDIFL",
    "WATCHDOG_FLAG"
};

Link* create(logger::Logger* logger, iop::intc::Intc* intc, scheduler::Scheduler* sched) {
    Link* link = new Link();

    link->logger = logger;
    link->logger_id = logger::register_source(logger, "link");

    // ARCNET CORE interrupt (possibly sent after INIMODE was set?)
    // TA bit set
    link->stsl = 1;

    // RECON bit set
    link->comr0 = 4;

    link->intc = intc;
    link->sched = sched;

    return link;
}

void send_irq(Link* link, uint16_t irq) {
    link->stsl |= irq & 0xff;
    link->stsh |= (irq >> 8) & 0xff;

    iop::intc::irq(link->intc, iop::intc::DEV9);
}

uint64_t read8(Link* link, uint32_t addr) {
    addr -= 0x10800000;

    uint32_t r = 0;

    switch (addr) {
        case PAD00: r = link->pad00; break;
        case COMR0: r = link->comr0; break;
        case PAD02: r = link->pad02; break;
        case COMR1: r = link->comr1; break;
        case PAD04: r = link->pad04; break;
        case COMR2: r = link->comr2; break;
        case PAD06: r = link->pad06; break;
        case COMR3: r = link->comr3; break;
        case PAD08: r = link->pad08; break;
        case COMR4: {
            uint32_t addr = link->ramadr;

            if (link->comr2 & COMR2_AUTOINC) {
                if (link->comr2 & COMR2_WRAPAR) {
                    link->ramadr = (link->ramadr & 0x3c0) | ((link->ramadr + 1) & 0x3f);
                } else {
                    link->ramadr = (link->ramadr + 1) & 0x3ff;
                }
            }

            // iris_debug(link, "Link: Read RAM[{:04x}] = {:02x}", addr, link->ram[addr]);

            r = link->ram[addr];
        } break;
        case PAD0A: r = link->pad0a; break;
        case COMR5: r = link->comr5; break;
        case PAD0C: r = link->pad0c; break;
        case COMR6: r = link->comr6; break;
        case PAD0E: r = link->pad0e; break;
        case COMR7: r = link->comr7; break;
        case NSTH: r = link->nsth; break;
        case NSTL: r = link->nstl; break;
        case STSH: r = link->stsh; break;
        case STSL: r = link->stsl; break;
        case MSKH: r = link->mskh; break;
        case MSKL: r = link->mskl; break;
        case PAD16: r = link->pad16; break;
        case ECCMD: r = link->eccmd; break;
        case MRSID: r = link->mrsid; break;
        case RSID: r = link->rsid; break;
        case PAD1A: r = link->pad1a; break;
        case SSID: r = link->ssid; break;
        case RXFHH: r = link->rxfhh; break;
        case RXFHL: r = link->rxfhl; break;
        case RXFLH: r = link->rxflh; break;
        case RXFLL: r = link->rxfll; break;
        case PAD20: r = link->pad20; break;
        case CMID: r = link->cmid; break;
        case MODEH: r = link->modeh; break;
        case MODEL: r = link->model; break;
        case CARRYH: r = link->carryh; break;
        case CARRYL: r = link->carryl; break;
        case RXMHH: r = link->rxmhh; break;
        case RXMHL: r = link->rxmhl; break;
        case RXMLH: r = link->rxmlh; break;
        case RXMLL: r = link->rxmll; break;
        case PAD2A: r = link->pad2a; break;
        case MAXID: r = link->maxid; break;
        case PAD2C: r = link->pad2c; break;
        case NID: r = link->nid; break;
        case PAD2E: r = link->pad2e; break;
        case PS: r = link->ps; break;
        case PAD30: r = link->pad30; break;
        case CKP: r = link->ckp; break;
        case NSTDIFH: r = link->nstdifh; break;
        case NSTDIFL: r = link->nstdifl; break;
        case WATCHDOG_FLAG: r = link->watchdog_flag; break;
    }

    if (addr != WATCHDOG_FLAG) {
        iris_debug(link, "Link: Read {} ({:08x}) {:08x}", g_reg_names[addr], addr, r);
    }

    return r;
}

void register_node(Link* link, int node, packet_handler handler, void* udata) {
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

void recv_reply(void* udata, int overshoot) {
    Link* link = (Link*)udata;

    Packet* tx = (Packet*)&link->ram[0x40];
    Packet* rx = (Packet*)&link->ram[tx->dst_node * 0x40];

    new (rx) Packet();

    // Set TA and TMA bits (transmitter available)
    link->stsl |= STSL_TA;
    link->comr0 |= COMR0_R_TA | COMR0_R_TMA;

    Node* node = &link->nodes[tx->dst_node];

    if (!node->handler) {
        iris_debug(link, "Link: Packet sent to disconnected node {} (cmd={:02x} cp={:02x})", tx->dst_node,
            tx->cmd,
            tx->cp);

        return;
    }

    // Set packet pending flag
    link->rxfll = 1 << tx->dst_node;

    // Get response from the requested node
    node->handler(node->udata, tx, rx);

    // Send DEV9 IRQ to IOP
    iop::intc::irq(link->intc, iop::intc::DEV9);
}

void write8(Link* link, uint32_t addr, uint64_t data) {
    addr -= 0x10800000;

    if (addr != WATCHDOG_FLAG) {
        iris_debug(link, "Link: Write {} ({:08x}) {:08x}", g_reg_names[addr], addr, data);
    }

    switch (addr) {
        case PAD00: link->pad00 = data; return;
        case COMR0: {
            link->comr0 &= ~(COMR0_R_RECON | COMR0_W_TA);
            link->comr0 |= data & (COMR0_R_RECON | COMR0_W_TA);

            if (data & 0xf) {
                send_irq(link, IRQ_COM);
            }
        } return;
        case PAD02: link->pad02 = data; return;
        case COMR1: link->comr1 = data; return;
        case PAD04: link->pad04 = data; return;
        case COMR2: {
            link->comr2 = data;
            link->ramadr = (link->ramadr & 0x3f) | ((data & 0xf) << 6);
        } return;
        case PAD06: link->pad06 = data; return;
        case COMR3: {
            link->comr3 = data;
            link->ramadr = (link->ramadr & 0x3c0) | (data & 0x3f);
        } return;
        case PAD08: link->pad08 = data; return;
        case COMR4: {
            uint32_t addr = link->ramadr;

            if (link->comr2 & COMR2_AUTOINC) {
                if (link->comr2 & COMR2_WRAPAR) {
                    link->ramadr = (link->ramadr + 1) & 0x3ff;
                } else {
                    link->ramadr = (link->ramadr & 0x3c0) | ((link->ramadr + 1) & 0x3f);
                }
            }

            // iris_debug(link, "Link: Write RAM[{:04x}] = {:02x}", addr, data);

            link->ram[addr] = data;
        } return;
        case PAD0A: link->pad0a = data; return;
        case COMR5: link->comr5 = data; return;
        case PAD0C: link->pad0c = data; return;
        case COMR6: link->comr6 = data; return;
        case PAD0E: link->pad0e = data; return;
        case COMR7: link->comr7 = data; return;
        case NSTH: link->nsth = data; return;
        case NSTL: link->nstl = data; return;
        case STSH: {
            link->stsh &= 0x30;
            link->stsh |= data & 0xcf;
        } return;
        case STSL: {
            link->stsl &= 0x09;
            link->stsl |= data & 0xf6;
        } return;
        case MSKH: {
            link->mskh = data;
        } return;
        case MSKL: {
            link->mskl = data;
        } return;
        case PAD16: link->pad16 = data; return;
        case ECCMD: { 
            link->eccmd = data;

            switch (link->eccmd) {
                case 0x03: {
                    scheduler::Event event;

                    event.callback = recv_reply;
                    event.udata = link;
                    event.cycles = 10000;
                    event.name = "Link reply";

                    link->stsl &= ~STSL_TA;
                    link->comr0 &= ~(COMR0_R_TA | COMR0_R_TMA);

                    scheduler::schedule(link->sched, event);
                } break;

                case 0x16: {
                    // Clear RECON bit
                    link->comr0 &= ~COMR0_W_RECON;
                } break;

                default: {
                    iris_debug(link, "Link: Unhandled EC command {:02x}", link->eccmd);
                } break;
            }
        } return;
        case MRSID: link->mrsid = data; return;
        case RSID: link->rsid = data; return;
        case PAD1A: link->pad1a = data; return;
        case SSID: link->ssid = data; return;

        // Writing clears receive flags
        case RXFHH: link->rxfhh &= ~data; return;
        case RXFHL: link->rxfhl &= ~data; return;
        case RXFLH: link->rxflh &= ~data; return;
        case RXFLL: link->rxfll &= ~data; return;
        case PAD20: link->pad20 = data; return;
        case CMID: link->cmid = data; return;
        case MODEH: {
            // Software reset
            if ((link->modeh & MODEH_INIMODE) != (data & MODEH_INIMODE)) {
                link->stsl = 0;
                link->stsh = 0;
                link->mskl = 0;
                link->mskh = 0;
                link->comr0 = 0;
                link->comr1 = 0;
            }

            link->modeh = data;
        } break;
        case MODEL: link->model = data; return;
        case CARRYH: link->carryh = data; return;
        case CARRYL: link->carryl = data; return;
        case RXMHH: link->rxmhh = data; return;
        case RXMHL: link->rxmhl = data; return;
        case RXMLH: link->rxmlh = data; return;
        case RXMLL: link->rxmll = data; return;
        case PAD2A: link->pad2a = data; return;
        case MAXID: link->maxid = data; return;
        case PAD2C: link->pad2c = data; return;
        case NID: link->nid = data; return;
        case PAD2E: link->pad2e = data; return;
        case PS: link->ps = data; return;
        case PAD30: link->pad30 = data; return;
        case CKP: link->ckp = data; return;
        case NSTDIFH: link->nstdifh = data; return;
        case NSTDIFL: link->nstdifl = data; return;
        case WATCHDOG_FLAG: link->watchdog_flag = data; return;
    }
}

void destroy(Link* link) {
    delete link;
}

}
