#pragma once

#include "intc.hpp"
#include "scheduler.hpp"
#include "bus_decl.hpp"

#include "fw/device.hpp"
#include "logger.hpp"

namespace iris::fw {

inline constexpr auto BASE_ADDR = 0x1f808400;

inline constexpr auto REG_SIZE = 0x200;
inline constexpr auto REG_COUNT = REG_SIZE / 4;

inline constexpr auto NODE_ID = 0x000;
inline constexpr auto CYCLE_TIME = 0x004;
inline constexpr auto CTRL0 = 0x008;
inline constexpr auto CTRL1 = 0x00c;
inline constexpr auto CTRL2 = 0x010;
inline constexpr auto PHY_ACCESS = 0x014;
inline constexpr auto INTR0 = 0x020;
inline constexpr auto INTR0_MASK = 0x024;
inline constexpr auto INTR1 = 0x028;
inline constexpr auto INTR1_MASK = 0x02c;
inline constexpr auto INTR2 = 0x030;
inline constexpr auto INTR2_MASK = 0x034;
inline constexpr auto DMAR = 0x038;
inline constexpr auto ACK_STAT = 0x03c;
inline constexpr auto UBUF_TX_NEXT = 0x040;
inline constexpr auto UBUF_TX_LAST = 0x044;
inline constexpr auto UBUF_TX_CLR = 0x048;
inline constexpr auto UBUF_RX_CLR = 0x04c;
inline constexpr auto UBUF_RX = 0x050;
inline constexpr auto UBUF_RX_LVL = 0x054;
inline constexpr auto REG_7C = 0x07c;
inline constexpr auto PHT_CTRL0 = 0x080;
inline constexpr auto PHT_SPLIT0 = 0x084;
inline constexpr auto PHT_REQ_HDR0 = 0x088;
inline constexpr auto PHT_REQ_HDR1 = 0x08c;
inline constexpr auto PHT_REQ_HDR2 = 0x090;
inline constexpr auto CH_SEL_HI0 = 0x094;
inline constexpr auto CH_SEL_LO0 = 0x098;
inline constexpr auto DMA_CTRL0 = 0x0b8;
inline constexpr auto DMA_RX_THRSH0 = 0x0bc;
inline constexpr auto DBUF_FIFO_LV0 = 0x0c0;
inline constexpr auto DBUF_TX_DATA0 = 0x0c4;
inline constexpr auto DBUF_RX_DATA = 0x0c8;
inline constexpr auto PHT_CTRL1 = 0x100;
inline constexpr auto PHT_SPLIT1 = 0x104;
inline constexpr auto CH_SEL_HI1 = 0x114;
inline constexpr auto CH_SEL_LO1 = 0x118;
inline constexpr auto DMA_CTRL1 = 0x138;
inline constexpr auto DMA_RX_THRSH1 = 0x13c;
inline constexpr auto DBUF_FIFO_LV1 = 0x140;
inline constexpr auto DBUF_TX_DATA1 = 0x144;

inline constexpr auto CTRL0_ROOT = 1 << 19;
inline constexpr auto CTRL0_BUS_ID_RST = 1 << 23;
inline constexpr auto CTRL0_RX_RST = 1 << 24;
inline constexpr auto CTRL0_TX_RST = 1 << 25;

inline constexpr auto PHY_ACCESS_WRITE = 1 << 30;
inline constexpr auto PHY_ACCESS_READ = 1u << 31;

inline constexpr auto INTR0_DRFR = 1 << 0;
inline constexpr auto INTR0_PB_CNT_R = 1 << 9;
inline constexpr auto INTR0_ACK_RCVD = 1 << 14;
inline constexpr auto INTR0_URX = 1 << 22;
inline constexpr auto INTR0_PHY_RST = 1 << 29;
inline constexpr auto INTR0_PHY_RRX = 1 << 30;

inline constexpr auto INTR1_UTD = 1 << 1;

inline constexpr auto PHT_CTRL_EWREQ = 1 << 16;
inline constexpr auto PHT_CTRL_ERREQ = 1 << 17;
inline constexpr auto PHT_CTRL_RST = 1 << 21;

inline constexpr auto DBUF_FIFO_RESET_TX = 1 << 15;
inline constexpr auto DBUF_FIFO_RESET_RX = 1u << 31;

inline constexpr auto PHY_REG01_IBR = 0x40;
inline constexpr auto PHY_REG05_ISBR = 0x40;
inline constexpr auto PHY_REG05_EN_ACCL = 0x02;
inline constexpr auto PHY_REG05_EN_MULTI = 0x01;

inline constexpr uint32_t REMOTE_PHY_ID = 0;
inline constexpr uint32_t LOCAL_PHY_ID = 1;

inline constexpr uint32_t SELF_ID_PORT_NOT_CONNECTED = 1;
inline constexpr uint32_t SELF_ID_PORT_PARENT = 2;
inline constexpr uint32_t SELF_ID_PORT_CHILD = 3;

inline constexpr uint32_t PHY_SELF_ID_PACKET = 0xe1;

inline constexpr uint32_t NODE_ID_RESET = (0x3ffu << 22) | (LOCAL_PHY_ID << 16) | 1u;
inline constexpr uint32_t REG_7C_VALUE = 0x10000001;

inline constexpr auto DBUF_RX_MAX = 64;

inline constexpr auto NUM_PORTS = 1;

enum {
    DEVICE_NONE = 0,
    DEVICE_P1IO,
    DEVICE_TYPE_COUNT
};

struct Fw {
    struct {
        iop::intc::Intc* intc;
        iop::bus::Bus* bus;
        scheduler::Scheduler* sched;
    } hw;

    uint32_t intr0;
    uint32_t intr1;
    uint32_t intr2;
    uint32_t intr0mask;
    uint32_t intr1mask;
    uint32_t intr2mask;
    uint32_t ctrl0;
    uint32_t ctrl1;
    uint32_t ctrl2;
    uint32_t dma_ctrl_sr0;
    uint32_t dma_ctrl_sr1;
    uint32_t phy_access;
    uint8_t phy_r[16];

    uint32_t reg[REG_COUNT];

    uint32_t dbuf_rx[DBUF_RX_MAX];
    int dbuf_rx_head;
    int dbuf_rx_size;

    device::Device device[NUM_PORTS];
    int device_type[NUM_PORTS];

    logger::Logger* logger = nullptr;
    size_t logger_id = 0;
};

Fw* create(logger::Logger* logger, iop::intc::Intc* intc, iop::bus::Bus* bus, scheduler::Scheduler* sched);
void reset(Fw* fw);
void destroy(Fw* fw);
uint64_t read32(Fw* fw, uint32_t addr);
void write32(Fw* fw, uint32_t addr, uint64_t data);

const char* device_type_name(int type);
int get_port_device(Fw* fw, int port);
void set_port_device(Fw* fw, int port, int type);

}
