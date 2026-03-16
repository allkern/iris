struct s14x_link;

#ifndef S14X_LINK_H
#define S14X_LINK_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdio.h>

#include "iop/intc.h"
#include "scheduler.h"

#define S14X_LINK_PAD00 0x00
#define S14X_LINK_COMR0 0x01
#define S14X_LINK_PAD02 0x02
#define S14X_LINK_COMR1 0x03
#define S14X_LINK_PAD04 0x04
#define S14X_LINK_COMR2 0x05
#define S14X_LINK_PAD06 0x06
#define S14X_LINK_COMR3 0x07
#define S14X_LINK_PAD08 0x08
#define S14X_LINK_COMR4 0x09
#define S14X_LINK_PAD0A 0x0a
#define S14X_LINK_COMR5 0x0b
#define S14X_LINK_PAD0C 0x0c
#define S14X_LINK_COMR6 0x0d
#define S14X_LINK_PAD0E 0x0e
#define S14X_LINK_COMR7 0x0f
#define S14X_LINK_NSTH 0x10
#define S14X_LINK_NSTL 0x11
#define S14X_LINK_STSH 0x12
#define S14X_LINK_STSL 0x13
#define S14X_LINK_MSKH 0x14
#define S14X_LINK_MSKL 0x15
#define S14X_LINK_PAD16 0x16
#define S14X_LINK_ECCMD 0x17
#define S14X_LINK_MRSID 0x18
#define S14X_LINK_RSID 0x19
#define S14X_LINK_PAD1A 0x1a
#define S14X_LINK_SSID 0x1b
#define S14X_LINK_RXFHH 0x1c
#define S14X_LINK_RXFHL 0x1d
#define S14X_LINK_RXFLH 0x1e
#define S14X_LINK_RXFLL 0x1f
#define S14X_LINK_PAD20 0x20
#define S14X_LINK_CMID 0x21
#define S14X_LINK_MODEH 0x22
#define S14X_LINK_MODEL 0x23
#define S14X_LINK_CARRYH 0x24
#define S14X_LINK_CARRYL 0x25
#define S14X_LINK_RXMHH 0x26
#define S14X_LINK_RXMHL 0x27
#define S14X_LINK_RXMLH 0x28
#define S14X_LINK_RXMLL 0x29
#define S14X_LINK_PAD2A 0x2a
#define S14X_LINK_MAXID 0x2b
#define S14X_LINK_PAD2C 0x2c
#define S14X_LINK_NID 0x2d
#define S14X_LINK_PAD2E 0x2e
#define S14X_LINK_PS 0x2f
#define S14X_LINK_PAD30 0x30
#define S14X_LINK_CKP 0x31
#define S14X_LINK_NSTDIFH 0x32
#define S14X_LINK_NSTDIFL 0x33
#define S14X_LINK_WATCHDOG_FLAG 0x34

#define S14X_LINK_COMR2_RDDATA  0x80
#define S14X_LINK_COMR2_AUTOINC 0x40
#define S14X_LINK_COMR2_WRAPAR  0x20
#define S14X_LINK_COMR2_PAGE    0x1f

#define S14X_LINK_COMR0_W_EXCNAK 8
#define S14X_LINK_COMR0_W_RECON 4
#define S14X_LINK_COMR0_W_NXTIDERR 2
#define S14X_LINK_COMR0_W_TA 1

#define S14X_LINK_COMR0_R_POR 0x10
#define S14X_LINK_COMR0_R_RECON 4
#define S14X_LINK_COMR0_R_TMA 2
#define S14X_LINK_COMR0_R_TA 1

#define S14X_LINK_IRQ_RXERR 0x8000
#define S14X_LINK_IRQ_CMIECC 0x4000
#define S14X_LINK_IRQ_NSTUNLOC 0x2000
#define S14X_LINK_IRQ_WARTERR 0x1000
#define S14X_LINK_IRQ_FRCV 0x0800
#define S14X_LINK_IRQ_RRCV 0x0300
#define S14X_LINK_IRQ_MRCV 0x0200
#define S14X_LINK_IRQ_SIDF 0x0100
#define S14X_LINK_IRQ_TKNRETF 0x0080
#define S14X_LINK_IRQ_ACKNAKF 0x0040
#define S14X_LINK_IRQ_HUBWDTO 0x0020
#define S14X_LINK_IRQ_CPERR 0x0010
#define S14X_LINK_IRQ_COM 0x0008
#define S14X_LINK_IRQ_FBENR 0x0003
#define S14X_LINK_IRQ_TXERR 0x0002
#define S14X_LINK_IRQ_TA 0x0001

#define S14X_LINK_MODEH_CMIERRMD 0x10
#define S14X_LINK_MODEH_NSTSEND 0x08
#define S14X_LINK_MODEH_NSTSTOP 0x04
#define S14X_LINK_MODEH_INIMODE 0x02
#define S14X_LINK_MODEH_TXEN 0x01
#define S14X_LINK_MODEL_ECRI 0x80
#define S14X_LINK_MODEL_BRE 0x40
#define S14X_LINK_MODEL_TXM 0x20
#define S14X_LINK_MODEL_RTO 0x10
#define S14X_LINK_MODEL_WDMD 0x08
#define S14X_LINK_MODEL_NTKNRTY 0x04
#define S14X_LINK_MODEL_NACKNAK 0x02
#define S14X_LINK_MODEL_NACLR 0x01

#define S14X_LINK_MSKH_RXERR 0x80
#define S14X_LINK_MSKH_CMIECC 0x40
#define S14X_LINK_MSKH_NSTUNLOC 0x20
#define S14X_LINK_MSKH_WARTERR 0x10
#define S14X_LINK_MSKH_FRCV 0x08
#define S14X_LINK_MSKH_RRCV 0x03
#define S14X_LINK_MSKH_MRCV 0x02
#define S14X_LINK_MSKH_SIDF 0x01
#define S14X_LINK_MSKL_TKNRETF 0x80
#define S14X_LINK_MSKL_ACKNAKF 0x40
#define S14X_LINK_MSKL_HUBWDTO 0x20
#define S14X_LINK_MSKL_CPERR 0x10
#define S14X_LINK_MSKL_COM 0x08
#define S14X_LINK_MSKL_FBENR 0x03
#define S14X_LINK_MSKL_TXERR 0x02
#define S14X_LINK_MSKL_TA 0x01

#define S14X_LINK_STSH_RXERR 0x80
#define S14X_LINK_STSH_CMIECC 0x40
#define S14X_LINK_STSH_NSTUNLOC 0x20
#define S14X_LINK_STSH_WARTERR 0x10
#define S14X_LINK_STSH_FRCV 0x08
#define S14X_LINK_STSH_RRCV 0x03
#define S14X_LINK_STSH_MRCV 0x02
#define S14X_LINK_STSH_SIDF 0x01
#define S14X_LINK_STSL_TKNRETF 0x80
#define S14X_LINK_STSL_ACKNAKF 0x40
#define S14X_LINK_STSL_HUBWDTO 0x20
#define S14X_LINK_STSL_CPERR 0x10
#define S14X_LINK_STSL_COM 0x08
#define S14X_LINK_STSL_FBENR 0x03
#define S14X_LINK_STSL_TXERR 0x02
#define S14X_LINK_STSL_TA 0x01

#define S14X_LINK_RAMSIZE 1024

// ARCNET packets are 64 bytes long
struct link_packet {
	uint8_t src_node;
	uint8_t dst_node;
	uint8_t cp;
	uint8_t unk03;
	uint8_t seq_number;
	uint8_t cmd;
	uint8_t data[0x39];
	uint8_t checksum;
};

typedef void (*link_packet_handler)(void*, struct link_packet*, struct link_packet*);

struct link_node {
	link_packet_handler handler;
	void* udata;
};

static uint8_t link_calculate_checksum(struct link_packet* packet);

struct s14x_link {
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

	uint8_t ram[S14X_LINK_RAMSIZE];

	uint32_t ramadr;

	struct link_node nodes[32];

	struct ps2_iop_intc* intc;
	struct sched_state* sched;
};

struct s14x_link* s14x_link_create(void);
void s14x_link_init(struct s14x_link* link, struct ps2_iop_intc* intc, struct sched_state* sched);
uint64_t s14x_link_read(struct s14x_link* link, uint32_t addr);
void s14x_link_write(struct s14x_link* link, uint32_t addr, uint64_t data);
void s14x_link_register_node(struct s14x_link* link, int node, link_packet_handler handler, void* udata);
void s14x_link_send_packet(struct s14x_link* link, struct link_packet packet);
void s14x_link_destroy(struct s14x_link* link);

uint8_t link_calculate_checksum(struct link_packet* packet);

#ifdef __cplusplus
}
#endif

#endif