#pragma once

#include "iop/intc.hpp"
#include "scheduler.hpp"
#include "logger.hpp"

namespace iris::s14x::link {

inline constexpr auto PAD00 = 0x00;
inline constexpr auto COMR0 = 0x01;
inline constexpr auto PAD02 = 0x02;
inline constexpr auto COMR1 = 0x03;
inline constexpr auto PAD04 = 0x04;
inline constexpr auto COMR2 = 0x05;
inline constexpr auto PAD06 = 0x06;
inline constexpr auto COMR3 = 0x07;
inline constexpr auto PAD08 = 0x08;
inline constexpr auto COMR4 = 0x09;
inline constexpr auto PAD0A = 0x0a;
inline constexpr auto COMR5 = 0x0b;
inline constexpr auto PAD0C = 0x0c;
inline constexpr auto COMR6 = 0x0d;
inline constexpr auto PAD0E = 0x0e;
inline constexpr auto COMR7 = 0x0f;
inline constexpr auto NSTH = 0x10;
inline constexpr auto NSTL = 0x11;
inline constexpr auto STSH = 0x12;
inline constexpr auto STSL = 0x13;
inline constexpr auto MSKH = 0x14;
inline constexpr auto MSKL = 0x15;
inline constexpr auto PAD16 = 0x16;
inline constexpr auto ECCMD = 0x17;
inline constexpr auto MRSID = 0x18;
inline constexpr auto RSID = 0x19;
inline constexpr auto PAD1A = 0x1a;
inline constexpr auto SSID = 0x1b;
inline constexpr auto RXFHH = 0x1c;
inline constexpr auto RXFHL = 0x1d;
inline constexpr auto RXFLH = 0x1e;
inline constexpr auto RXFLL = 0x1f;
inline constexpr auto PAD20 = 0x20;
inline constexpr auto CMID = 0x21;
inline constexpr auto MODEH = 0x22;
inline constexpr auto MODEL = 0x23;
inline constexpr auto CARRYH = 0x24;
inline constexpr auto CARRYL = 0x25;
inline constexpr auto RXMHH = 0x26;
inline constexpr auto RXMHL = 0x27;
inline constexpr auto RXMLH = 0x28;
inline constexpr auto RXMLL = 0x29;
inline constexpr auto PAD2A = 0x2a;
inline constexpr auto MAXID = 0x2b;
inline constexpr auto PAD2C = 0x2c;
inline constexpr auto NID = 0x2d;
inline constexpr auto PAD2E = 0x2e;
inline constexpr auto PS = 0x2f;
inline constexpr auto PAD30 = 0x30;
inline constexpr auto CKP = 0x31;
inline constexpr auto NSTDIFH = 0x32;
inline constexpr auto NSTDIFL = 0x33;
inline constexpr auto WATCHDOG_FLAG = 0x34;

inline constexpr auto COMR2_RDDATA = 0x80;
inline constexpr auto COMR2_AUTOINC = 0x40;
inline constexpr auto COMR2_WRAPAR = 0x20;
inline constexpr auto COMR2_PAGE = 0x1f;

inline constexpr auto COMR0_W_EXCNAK = 8;
inline constexpr auto COMR0_W_RECON = 4;
inline constexpr auto COMR0_W_NXTIDERR = 2;
inline constexpr auto COMR0_W_TA = 1;

inline constexpr auto COMR0_R_POR = 0x10;
inline constexpr auto COMR0_R_RECON = 4;
inline constexpr auto COMR0_R_TMA = 2;
inline constexpr auto COMR0_R_TA = 1;

inline constexpr auto IRQ_RXERR = 0x8000;
inline constexpr auto IRQ_CMIECC = 0x4000;
inline constexpr auto IRQ_NSTUNLOC = 0x2000;
inline constexpr auto IRQ_WARTERR = 0x1000;
inline constexpr auto IRQ_FRCV = 0x0800;
inline constexpr auto IRQ_RRCV = 0x0300;
inline constexpr auto IRQ_MRCV = 0x0200;
inline constexpr auto IRQ_SIDF = 0x0100;
inline constexpr auto IRQ_TKNRETF = 0x0080;
inline constexpr auto IRQ_ACKNAKF = 0x0040;
inline constexpr auto IRQ_HUBWDTO = 0x0020;
inline constexpr auto IRQ_CPERR = 0x0010;
inline constexpr auto IRQ_COM = 0x0008;
inline constexpr auto IRQ_FBENR = 0x0003;
inline constexpr auto IRQ_TXERR = 0x0002;
inline constexpr auto IRQ_TA = 0x0001;

inline constexpr auto MODEH_CMIERRMD = 0x10;
inline constexpr auto MODEH_NSTSEND = 0x08;
inline constexpr auto MODEH_NSTSTOP = 0x04;
inline constexpr auto MODEH_INIMODE = 0x02;
inline constexpr auto MODEH_TXEN = 0x01;
inline constexpr auto MODEL_ECRI = 0x80;
inline constexpr auto MODEL_BRE = 0x40;
inline constexpr auto MODEL_TXM = 0x20;
inline constexpr auto MODEL_RTO = 0x10;
inline constexpr auto MODEL_WDMD = 0x08;
inline constexpr auto MODEL_NTKNRTY = 0x04;
inline constexpr auto MODEL_NACKNAK = 0x02;
inline constexpr auto MODEL_NACLR = 0x01;

inline constexpr auto MSKH_RXERR = 0x80;
inline constexpr auto MSKH_CMIECC = 0x40;
inline constexpr auto MSKH_NSTUNLOC = 0x20;
inline constexpr auto MSKH_WARTERR = 0x10;
inline constexpr auto MSKH_FRCV = 0x08;
inline constexpr auto MSKH_RRCV = 0x03;
inline constexpr auto MSKH_MRCV = 0x02;
inline constexpr auto MSKH_SIDF = 0x01;
inline constexpr auto MSKL_TKNRETF = 0x80;
inline constexpr auto MSKL_ACKNAKF = 0x40;
inline constexpr auto MSKL_HUBWDTO = 0x20;
inline constexpr auto MSKL_CPERR = 0x10;
inline constexpr auto MSKL_COM = 0x08;
inline constexpr auto MSKL_FBENR = 0x03;
inline constexpr auto MSKL_TXERR = 0x02;
inline constexpr auto MSKL_TA = 0x01;

inline constexpr auto STSH_RXERR = 0x80;
inline constexpr auto STSH_CMIECC = 0x40;
inline constexpr auto STSH_NSTUNLOC = 0x20;
inline constexpr auto STSH_WARTERR = 0x10;
inline constexpr auto STSH_FRCV = 0x08;
inline constexpr auto STSH_RRCV = 0x03;
inline constexpr auto STSH_MRCV = 0x02;
inline constexpr auto STSH_SIDF = 0x01;
inline constexpr auto STSL_TKNRETF = 0x80;
inline constexpr auto STSL_ACKNAKF = 0x40;
inline constexpr auto STSL_HUBWDTO = 0x20;
inline constexpr auto STSL_CPERR = 0x10;
inline constexpr auto STSL_COM = 0x08;
inline constexpr auto STSL_FBENR = 0x03;
inline constexpr auto STSL_TXERR = 0x02;
inline constexpr auto STSL_TA = 0x01;

inline constexpr auto RAMSIZE = 1024;

// ARCNET packets are 64 bytes long
struct Packet {
    union {
        uint8_t raw[64];

        struct {
            // Offset 00h - Node of the sender
            uint8_t src_node;

            // Offset 01h - Node of the receiver
            uint8_t dst_node;

            // Offset 02h - Continuation Pointer (data offset within packet)
            uint8_t cp;

            // Offset 03h - Unknown (must be 00h)
            uint8_t unk03;

            // Offset 04h - Packet number in a sequence
            uint8_t seq_number;

            // Offset 05h - I/O board command
            uint8_t cmd;

            // Offset 06h-3Eh - Command payload/data
            uint8_t data[0x39];

            // Offsset 3Fh - Checksum of all the data bytes
            uint8_t checksum;
        };
    };
};

typedef void (*packet_handler)(void*, Packet*, Packet*);

struct Node {
    packet_handler handler;
    void* udata;
};

struct Link {
    uint8_t pad00;
    uint8_t comr0;
    uint8_t pad02;
    uint8_t comr1;
    uint8_t pad04;
    uint8_t comr2;
    uint8_t pad06;
    uint8_t comr3;
    uint8_t pad08;
    uint8_t comr4;
    uint8_t pad0a;
    uint8_t comr5;
    uint8_t pad0c;
    uint8_t comr6;
    uint8_t pad0e;
    uint8_t comr7;
    uint8_t nsth;
    uint8_t nstl;
    uint8_t stsh;
    uint8_t stsl;
    uint8_t mskh;
    uint8_t mskl;
    uint8_t pad16;
    uint8_t eccmd;
    uint8_t mrsid;
    uint8_t rsid;
    uint8_t pad1a;
    uint8_t ssid;
    uint8_t rxfhh;
    uint8_t rxfhl;
    uint8_t rxflh;
    uint8_t rxfll;
    uint8_t pad20;
    uint8_t cmid;
    uint8_t modeh;
    uint8_t model;
    uint8_t carryh;
    uint8_t carryl;
    uint8_t rxmhh;
    uint8_t rxmhl;
    uint8_t rxmlh;
    uint8_t rxmll;
    uint8_t pad2a;
    uint8_t maxid;
    uint8_t pad2c;
    uint8_t nid;
    uint8_t pad2e;
    uint8_t ps;
    uint8_t pad30;
    uint8_t ckp;
    uint8_t nstdifh;
    uint8_t nstdifl;
    uint8_t watchdog_flag;

    uint8_t ram[RAMSIZE];

    uint32_t ramadr;

    Node nodes[32];

    iop::intc::Intc* intc;
    scheduler::Scheduler* sched;

    logger::Logger* logger = nullptr;
    size_t logger_id = 0;
};

Link* create(logger::Logger* logger, iop::intc::Intc* intc, scheduler::Scheduler* sched);
uint64_t read8(Link* link, uint32_t addr);
void write8(Link* link, uint32_t addr, uint64_t data);
void register_node(Link* link, int node, packet_handler handler, void* udata);
void send_packet(Link* link, Packet packet);
void send_from_node(Link* link, int node, const Packet& packet);
void destroy(Link* link);

uint8_t calculate_checksum(Packet* packet);

}
