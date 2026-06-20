#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "smap.h"

#define SMAP_REGOFF 0x100

// link up, auto-negotiated 100M full duplex, remote-fault clear
#define SMAP_PHY_BMSR_VAL \
    (0x7800 | 0x0040 | 0x0008 | 0x0001 | SMAP_PHY_BMSR_ANCP | SMAP_PHY_BMSR_LINK)
#define SMAP_PHY_ANLPAR_VAL \
    (0x4000 | 0x0001 | SMAP_PHY_ANAR_TX_FD | SMAP_PHY_ANAR_TX | \
     SMAP_PHY_ANAR_10_FD | SMAP_PHY_ANAR_10)
#define SMAP_PHY_PHYSTS_VAL \
    (SMAP_PHY_STS_LINK | SMAP_PHY_STS_FDX | SMAP_PHY_STS_ANCP)

static inline uint32_t smap_swap16(uint32_t v) {
    return (v >> 16) | (v << 16);
}

// PHY (National DP83846A "DsPHYTER"), commands sent through STA_CTRL
static uint16_t smap_phy_read(struct ps2_smap* smap, int reg) {
    switch (reg) {
        case SMAP_DsPHYTER_BMCR: return smap->phy[SMAP_DsPHYTER_BMCR];
        case SMAP_DsPHYTER_BMSR: return SMAP_PHY_BMSR_VAL;
        case SMAP_DsPHYTER_PHYIDR1: return SMAP_PHY_IDR1_VAL;
        case SMAP_DsPHYTER_PHYIDR2: return SMAP_PHY_IDR2_VAL;
        case SMAP_DsPHYTER_ANAR: return smap->phy[SMAP_DsPHYTER_ANAR];
        case SMAP_DsPHYTER_ANLPAR: return SMAP_PHY_ANLPAR_VAL;
        case SMAP_DsPHYTER_PHYSTS: return SMAP_PHY_PHYSTS_VAL;
        case SMAP_DsPHYTER_10BTSCR: return smap->phy[SMAP_DsPHYTER_10BTSCR];
    }

    return 0;
}

static void smap_phy_write(struct ps2_smap* smap, int reg, uint16_t data) {
    switch (reg) {
        case SMAP_DsPHYTER_BMCR: {
            smap->phy[SMAP_DsPHYTER_BMCR] = data & ~(SMAP_PHY_BMCR_RST | SMAP_PHY_BMCR_RSAN);
        } return;

        case SMAP_DsPHYTER_ANAR: {
            smap->phy[SMAP_DsPHYTER_ANAR] = data;
        } return;

        case SMAP_DsPHYTER_10BTSCR: {
            smap->phy[SMAP_DsPHYTER_10BTSCR] = data;
        } return;
    }
}

static void smap_tx_irq_event(void* udata, int overshoot) {
    struct ps2_smap* smap = (struct ps2_smap*)udata;

    ps2_speed_send_irq(smap->speed, SMAP_INTR_TXEND | SMAP_INTR_TXDNV);
}

static void smap_transmit(struct ps2_smap* smap) {
    while (smap->tx_bd[smap->tx_bd_index % SMAP_BD_MAX_ENTRY].ctrl & SMAP_BD_TX_READY) {
        struct smap_bd* bd = &smap->tx_bd[smap->tx_bd_index % SMAP_BD_MAX_ENTRY];

        bd->ctrl &= ~SMAP_BD_TX_READY;

        if (smap->tx_frame_cnt)
            smap->tx_frame_cnt--;

        smap->tx_bd_index++;
    }

    struct sched_event event;

    event.callback = smap_tx_irq_event;
    event.udata = smap;
    event.name = "smap tx end";
    event.cycles = 1000;

    sched_schedule(smap->speed->sched, event);
}

// EMAC3 (IBM EMAC core)
static uint32_t smap_emac3_read(struct ps2_smap* smap, uint32_t reg) {
    switch (reg) {
        case SMAP_R_EMAC3_MODE0:
            return smap->emac3_mode0 | SMAP_E3_RXMAC_IDLE | SMAP_E3_TXMAC_IDLE;
        case SMAP_R_EMAC3_MODE1: return smap->emac3_mode1;
        case SMAP_R_EMAC3_TxMODE0: return 0;
        case SMAP_R_EMAC3_TxMODE1: return smap->emac3_txmode1;
        case SMAP_R_EMAC3_RxMODE: return smap->emac3_rxmode;
        case SMAP_R_EMAC3_INTR_STAT: return smap->emac3_intr_stat;
        case SMAP_R_EMAC3_INTR_ENABLE: return smap->emac3_intr_enable;
        case SMAP_R_EMAC3_ADDR_HI: return smap->emac3_addr_hi;
        case SMAP_R_EMAC3_ADDR_LO: return smap->emac3_addr_lo;
        case SMAP_R_EMAC3_VLAN_TPID: return smap->emac3_vlan_tpid;
        case SMAP_R_EMAC3_VLAN_TCI: return smap->emac3_vlan_tci;
        case SMAP_R_EMAC3_PAUSE_TIMER: return smap->emac3_pause_timer;
        case SMAP_R_EMAC3_INDIVID_HASH1: return smap->emac3_individ_hash1;
        case SMAP_R_EMAC3_INDIVID_HASH2: return smap->emac3_individ_hash2;
        case SMAP_R_EMAC3_INDIVID_HASH3: return smap->emac3_individ_hash3;
        case SMAP_R_EMAC3_INDIVID_HASH4: return smap->emac3_individ_hash4;
        case SMAP_R_EMAC3_GROUP_HASH1: return smap->emac3_group_hash1;
        case SMAP_R_EMAC3_GROUP_HASH2: return smap->emac3_group_hash2;
        case SMAP_R_EMAC3_GROUP_HASH3: return smap->emac3_group_hash3;
        case SMAP_R_EMAC3_GROUP_HASH4: return smap->emac3_group_hash4;
        case SMAP_R_EMAC3_LAST_SA_HI: return smap->emac3_last_sa_hi;
        case SMAP_R_EMAC3_LAST_SA_LO: return smap->emac3_last_sa_lo;
        case SMAP_R_EMAC3_INTER_FRAME_GAP:  return smap->emac3_inter_frame_gap;
        case SMAP_R_EMAC3_STA_CTRL: return smap->emac3_sta_ctrl;
        case SMAP_R_EMAC3_TX_THRESHOLD: return smap->emac3_tx_threshold;
        case SMAP_R_EMAC3_RX_WATERMARK: return smap->emac3_rx_watermark;
        case SMAP_R_EMAC3_TX_OCTETS: return 0;
        case SMAP_R_EMAC3_RX_OCTETS: return 0;
    }

    printf("smap: Unhandled EMAC3 read reg %02x\n", reg);

    return 0;
}

static void smap_emac3_write(struct ps2_smap* smap, uint32_t reg, uint32_t data) {
    switch (reg) {
        case SMAP_R_EMAC3_MODE0: {
            if (data & SMAP_E3_SOFT_RESET) {
                smap->emac3_mode0 = 0;
                smap->emac3_intr_stat = 0;
            } else {
                smap->emac3_mode0 = data & ~SMAP_E3_SOFT_RESET;
            }
        } return;
        case SMAP_R_EMAC3_MODE1: smap->emac3_mode1 = data; return;
        case SMAP_R_EMAC3_TxMODE0: {
            // printf("smap: EMAC3 TxMODE0 write %08x\n", data);

            if (data & (SMAP_E3_TX_GNP_0 | SMAP_E3_TX_GNP_1)) {
                smap_transmit(smap);
            }
        } return;
        case SMAP_R_EMAC3_TxMODE1: smap->emac3_txmode1 = data; return;
        case SMAP_R_EMAC3_RxMODE: smap->emac3_rxmode = data; return;
        case SMAP_R_EMAC3_INTR_STAT: smap->emac3_intr_stat &= ~data; return;
        case SMAP_R_EMAC3_INTR_ENABLE: smap->emac3_intr_enable = data; return;
        case SMAP_R_EMAC3_ADDR_HI: smap->emac3_addr_hi = data; return;
        case SMAP_R_EMAC3_ADDR_LO: smap->emac3_addr_lo = data; return;
        case SMAP_R_EMAC3_VLAN_TPID: smap->emac3_vlan_tpid = data; return;
        case SMAP_R_EMAC3_VLAN_TCI: smap->emac3_vlan_tci = data; return;
        case SMAP_R_EMAC3_PAUSE_TIMER: smap->emac3_pause_timer = data; return;
        case SMAP_R_EMAC3_INDIVID_HASH1: smap->emac3_individ_hash1 = data; return;
        case SMAP_R_EMAC3_INDIVID_HASH2: smap->emac3_individ_hash2 = data; return;
        case SMAP_R_EMAC3_INDIVID_HASH3: smap->emac3_individ_hash3 = data; return;
        case SMAP_R_EMAC3_INDIVID_HASH4: smap->emac3_individ_hash4 = data; return;
        case SMAP_R_EMAC3_GROUP_HASH1: smap->emac3_group_hash1 = data; return;
        case SMAP_R_EMAC3_GROUP_HASH2: smap->emac3_group_hash2 = data; return;
        case SMAP_R_EMAC3_GROUP_HASH3: smap->emac3_group_hash3 = data; return;
        case SMAP_R_EMAC3_GROUP_HASH4: smap->emac3_group_hash4 = data; return;
        case SMAP_R_EMAC3_INTER_FRAME_GAP: smap->emac3_inter_frame_gap = data; return;
        case SMAP_R_EMAC3_TX_THRESHOLD: smap->emac3_tx_threshold = data; return;
        case SMAP_R_EMAC3_RX_WATERMARK: smap->emac3_rx_watermark = data; return;

        // MII
        case SMAP_R_EMAC3_STA_CTRL: {
            // printf("smap: EMAC3 STA_CTRL write %08x\n", data);

            int phy_reg = data & SMAP_E3_PHY_REG_ADDR_MSK;
            int cmd = data & (3 << SMAP_E3_PHY_STA_CMD_BITSFT);
            uint16_t result = 0;

            if (cmd == SMAP_E3_PHY_WRITE) {
                smap_phy_write(smap, phy_reg, (data >> SMAP_E3_PHY_DATA_BITSFT) & 0xffff);
            } else if (cmd == SMAP_E3_PHY_READ) {
                result = smap_phy_read(smap, phy_reg);
            }

            smap->emac3_sta_ctrl = (data & 0x00003fffu) | SMAP_E3_PHY_OP_COMP
                | ((uint32_t)result << SMAP_E3_PHY_DATA_BITSFT);

            return;
        }
    }

    printf("smap: Unhandled EMAC3 write reg %02x (%08x)\n", reg, data);
}

static struct smap_bd* smap_bd_ptr(struct ps2_smap* smap, uint32_t off) {
    uint32_t boff = off - SMAP_BD_REGBASE;
    uint32_t index = boff / sizeof(struct smap_bd);

    if (index < SMAP_BD_MAX_ENTRY)
        return &smap->tx_bd[index];

    if (index < 2 * SMAP_BD_MAX_ENTRY)
        return &smap->rx_bd[index - SMAP_BD_MAX_ENTRY];

    return NULL;
}

static uint32_t smap_reg_read(struct ps2_smap* smap, uint32_t off) {
    switch (off) {
        case SMAP_R_BD_MODE: return smap->bd_mode;
        case SMAP_R_INTR_CLR: return smap->speed->intr_stat & SMAP_INTR_BITMSK;
        case SMAP_R_TXFIFO_CTRL: return smap->txfifo_ctrl & ~SMAP_TXFIFO_RESET;
        case SMAP_R_TXFIFO_WR_PTR: return smap->tx_wr_ptr;
        case SMAP_R_TXFIFO_SIZE: return smap->tx_size;
        case SMAP_R_TXFIFO_FRAME_CNT: return smap->tx_frame_cnt;
        case SMAP_R_RXFIFO_CTRL: return smap->rxfifo_ctrl & ~SMAP_RXFIFO_RESET;
        case SMAP_R_RXFIFO_RD_PTR: return smap->rx_rd_ptr;
        case SMAP_R_RXFIFO_SIZE: return smap->rx_size;
        case SMAP_R_RXFIFO_FRAME_CNT: return smap->rx_frame_cnt;
        case SMAP_R_FIFO_ADDR: return 0;
    }

    printf("smap: Unhandled reg read off=%04x\n", off);

    return 0;
}

static void smap_reg_write(struct ps2_smap* smap, uint32_t off, uint32_t data) {
    switch (off) {
        case SMAP_R_BD_MODE: smap->bd_mode = data; return;
        case SMAP_R_INTR_CLR: {
            smap->speed->intr_stat &= ~(data & SMAP_INTR_BITMSK);
        } return;

        case SMAP_R_TXFIFO_CTRL: {
            smap->txfifo_ctrl = data & ~SMAP_TXFIFO_RESET;

            if (data & SMAP_TXFIFO_RESET) {
                smap->tx_wr_ptr = 0;
                smap->tx_frame_cnt = 0;
            }
        } return;

        case SMAP_R_TXFIFO_WR_PTR: smap->tx_wr_ptr = data; return;
        case SMAP_R_TXFIFO_SIZE: smap->tx_size = data; return;
        case SMAP_R_TXFIFO_FRAME_INC: smap->tx_frame_cnt++; return;

        case SMAP_R_RXFIFO_CTRL: {
            smap->rxfifo_ctrl = data & ~SMAP_RXFIFO_RESET;

            if (data & SMAP_RXFIFO_RESET) {
                smap->rx_rd_ptr = 0;
                smap->rx_frame_cnt = 0;
            }
        } return;

        case SMAP_R_RXFIFO_RD_PTR: smap->rx_rd_ptr = data; return;
        case SMAP_R_RXFIFO_SIZE: smap->rx_size = data; return;

        case SMAP_R_RXFIFO_FRAME_DEC: {
            if (smap->rx_frame_cnt) smap->rx_frame_cnt--;
        } return;

        case SMAP_R_FIFO_ADDR: return;
    }

    printf("smap: Unhandled reg write off=%04x (%08x)\n", off, data);
}

struct ps2_smap* ps2_smap_create() {
    return malloc(sizeof(struct ps2_smap));
}

int ps2_smap_init(struct ps2_smap* smap, struct ps2_speed* speed) {
    memset(smap, 0, sizeof(struct ps2_smap));

    smap->speed = speed;

    smap->emac3_sta_ctrl = SMAP_E3_PHY_OP_COMP;

    smap->phy[SMAP_DsPHYTER_BMCR] =
        SMAP_PHY_BMCR_ANEN | SMAP_PHY_BMCR_100M | SMAP_PHY_BMCR_DUPM;

    smap->phy[SMAP_DsPHYTER_ANAR] =
        SMAP_PHY_ANAR_TX_FD | SMAP_PHY_ANAR_TX |
        SMAP_PHY_ANAR_10_FD | SMAP_PHY_ANAR_10 | 0x0001;

    return 1;
}

uint64_t ps2_smap_read8(struct ps2_smap* smap, uint32_t addr) {
    return smap_reg_read(smap, addr - SMAP_REGOFF);
}

uint64_t ps2_smap_read16(struct ps2_smap* smap, uint32_t addr) {
    uint32_t off = addr - SMAP_REGOFF;

    if (off >= SMAP_EMAC3_REGBASE && off < SMAP_BD_REGBASE) {
        uint32_t eoff = off - SMAP_EMAC3_REGBASE;
        uint32_t v = smap_emac3_read(smap, eoff & ~3u);

        return (eoff & 2) ? (v & 0xffff) : (v >> 16);
    }

    if (off >= SMAP_BD_REGBASE) {
        struct smap_bd* bd = smap_bd_ptr(smap, off);

        if (!bd)
            return 0;

        return *(uint16_t*)((uint8_t*)bd + ((off - SMAP_BD_REGBASE) % sizeof(struct smap_bd)));
    }

    return smap_reg_read(smap, off);
}

uint64_t ps2_smap_read32(struct ps2_smap* smap, uint32_t addr) {
    uint32_t off = addr - SMAP_REGOFF;

    if (off >= SMAP_EMAC3_REGBASE && off < SMAP_BD_REGBASE) {
        return smap_swap16(smap_emac3_read(smap, (off - SMAP_EMAC3_REGBASE) & ~3u));
    }

    if (off >= SMAP_BD_REGBASE) {
        struct smap_bd* bd = smap_bd_ptr(smap, off);

        if (!bd)
            return 0;

        return *(uint32_t*)((uint8_t*)bd + ((off - SMAP_BD_REGBASE) & 4u));
    }

    if (off == SMAP_R_RXFIFO_DATA || off == SMAP_R_FIFO_DATA)
        return ps2_smap_fifo_read(smap);

    return smap_reg_read(smap, off);
}

void ps2_smap_write8(struct ps2_smap* smap, uint32_t addr, uint64_t data) {
    smap_reg_write(smap, addr - SMAP_REGOFF, (uint8_t)data);
}

void ps2_smap_write16(struct ps2_smap* smap, uint32_t addr, uint64_t data) {
    uint32_t off = addr - SMAP_REGOFF;

    if (off >= SMAP_EMAC3_REGBASE && off < SMAP_BD_REGBASE) {
        uint32_t eoff = off - SMAP_EMAC3_REGBASE;
        uint32_t reg = eoff & ~3u;
        uint32_t v = smap_emac3_read(smap, reg);

        if (eoff & 2)
            v = (v & 0xffff0000u) | (uint16_t)data;
        else
            v = (v & 0x0000ffffu) | ((uint32_t)(uint16_t)data << 16);

        smap_emac3_write(smap, reg, v);

        return;
    }

    if (off >= SMAP_BD_REGBASE) {
        struct smap_bd* bd = smap_bd_ptr(smap, off);

        if (bd)
            *(uint16_t*)((uint8_t*)bd + ((off - SMAP_BD_REGBASE) % sizeof(struct smap_bd))) = (uint16_t)data;

        return;
    }

    smap_reg_write(smap, off, (uint16_t)data);
}

void ps2_smap_write32(struct ps2_smap* smap, uint32_t addr, uint64_t data) {
    uint32_t off = addr - SMAP_REGOFF;

    if (off >= SMAP_EMAC3_REGBASE && off < SMAP_BD_REGBASE) {
        smap_emac3_write(smap, (off - SMAP_EMAC3_REGBASE) & ~3u, smap_swap16((uint32_t)data));

        return;
    }

    if (off >= SMAP_BD_REGBASE) {
        struct smap_bd* bd = smap_bd_ptr(smap, off);

        if (bd)
            *(uint32_t*)((uint8_t*)bd + ((off - SMAP_BD_REGBASE) & 4u)) = (uint32_t)data;

        return;
    }

    if (off == SMAP_R_TXFIFO_DATA || off == SMAP_R_FIFO_DATA) {
        ps2_smap_fifo_write(smap, (uint32_t)data);

        return;
    }

    smap_reg_write(smap, off, (uint32_t)data);
}

int ps2_smap_dma_pending(struct ps2_smap* smap) {
    return ((smap->txfifo_ctrl | smap->rxfifo_ctrl) & SMAP_TXFIFO_DMAEN) != 0;
}

void ps2_smap_fifo_write(struct ps2_smap* smap, uint32_t data) {
    (void)data;

    smap->tx_wr_ptr = (smap->tx_wr_ptr + 4) & (SMAP_TX_BUFSIZE - 1);
}

uint32_t ps2_smap_fifo_read(struct ps2_smap* smap) {
    smap->rx_rd_ptr = (smap->rx_rd_ptr + 4) & (SMAP_RX_BUFSIZE - 1);

    return 0;
}

void ps2_smap_dma_complete(struct ps2_smap* smap) {
    smap->txfifo_ctrl &= ~SMAP_TXFIFO_DMAEN;
    smap->rxfifo_ctrl &= ~SMAP_RXFIFO_DMAEN;
}

void ps2_smap_destroy(struct ps2_smap* smap) {
    free(smap);
}
