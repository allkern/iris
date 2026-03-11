#ifndef S14X_LINK_H
#define S14X_LINK_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdio.h>

struct s14x_link {
	uint8_t m_pad00;
	uint8_t m_unk01;
	uint8_t m_pad02;
	uint8_t m_unk03;
	uint8_t m_pad04;
	uint8_t m_node_unk05;
	uint8_t m_pad06;
	uint8_t m_unk07;
	uint8_t m_pad08;
	uint8_t m_unk09;
	uint8_t m_pad0A;
	uint8_t m_pad0B;
	uint8_t m_pad0C;
	uint8_t m_unk0D;
	uint8_t m_pad0E;
	uint8_t m_pad0F;
	uint8_t m_pad10;
	uint8_t m_pad11;
	uint8_t m_stsH_unk12;
	uint8_t m_stsL_unk13;
	uint8_t m_unk14;
	uint8_t m_unk15;
	uint8_t m_pad16;
	uint8_t m_unk17;
	uint8_t m_pad18;
	uint8_t m_pad19;
	uint8_t m_pad1A;
	uint8_t m_pad1B;
	uint8_t m_unk1C;
	uint8_t m_unk1D;
	uint8_t m_rxfc_hi_unk1E;
	uint8_t m_rxfc_lo_unk1F;
	uint8_t m_pad20;
	uint8_t m_unk21;
	uint8_t m_unk22;
	uint8_t m_unk23;
	uint8_t m_unk24;
	uint8_t m_unk25;
	uint8_t m_pad26;
	uint8_t m_pad27;
	uint8_t m_unk28;
	uint8_t m_unk29;
	uint8_t m_pad2A;
	uint8_t m_maxnode_unk2B;
	uint8_t m_pad2C;
	uint8_t m_mynode_unk2D;
	uint8_t m_pad2E;
	uint8_t m_unk2F;
	uint8_t m_pad30;
	uint8_t m_unk31;
	uint8_t m_pad32;
	uint8_t m_pad33;
	uint8_t m_watchdog_flag_unk34;
};

struct s14x_link* s14x_link_create(void);
void s14x_link_init(struct s14x_link* link);
uint64_t s14x_link_read(struct s14x_link* link, uint32_t addr);
void s14x_link_write(struct s14x_link* link, uint32_t addr, uint64_t data);
void s14x_link_destroy(struct s14x_link* link);

#ifdef __cplusplus
}
#endif

#endif