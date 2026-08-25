#include <new>
#include <cstring>

#include "fw.hpp"
#include "kp1/p1io.hpp"

namespace iris::fw {

static const struct {
    const char* name;
    void (*create)(device::Device* dev);
} fw_device_types[DEVICE_TYPE_COUNT] = {
    { "None", nullptr },
    { "Konami Python 1 I/O board", kp1::p1io::create },
};

const char* device_type_name(int type) {
    if (type < 0 || type >= DEVICE_TYPE_COUNT)
        return NULL;

    return fw_device_types[type].name;
}

int get_port_device(Fw* fw, int port) {
    if (port < 0 || port >= NUM_PORTS)
        return DEVICE_NONE;

    return fw->device_type[port];
}

void set_port_device(Fw* fw, int port, int type) {
    if (port < 0 || port >= NUM_PORTS)
        return;

    if (type < 0 || type >= DEVICE_TYPE_COUNT)
        return;

    device::Device* dev = &fw->device[port];

    device::free(dev);

    dev->logger = fw->logger;

    fw->device_type[port] = type;

    if (fw_device_types[type].create) {
        fw_device_types[type].create(dev);

        iris_info(fw, "port {} device set to {}", port, fw_device_types[type].name);
    }
}

static int has_remote_node(Fw* fw) {
    for (int i = 0; i < NUM_PORTS; i++) {
        if (fw->device[i].connected)
            return 1;
    }

    return 0;
}

static void init_phy(Fw* fw) {
    memset(fw->phy_r, 0, sizeof(fw->phy_r));

    fw->phy_r[0x00] = (uint8_t)((LOCAL_PHY_ID << 2) | 0x03);
    fw->phy_r[0x01] = 0x3f;
    fw->phy_r[0x02] = 0x03;
    fw->phy_r[0x03] = 0x40;
    fw->phy_r[0x04] = 0x80;
    fw->phy_r[0x05] = PHY_REG05_EN_ACCL | PHY_REG05_EN_MULTI;
}

static void raise_intr0(Fw* fw, uint32_t bits) {
    fw->intr0 |= bits;

    if (fw->intr0mask & bits)
        iop::intc::irq(fw->hw.intc, iop::intc::FWRE);
}

static void update_dbuf_rx_level(Fw* fw) {
    fw->reg[DBUF_FIFO_LV0 >> 2] = (uint32_t)(fw->dbuf_rx_size * sizeof(uint32_t)) << 16;
}

static void queue_dbuf_rx(Fw* fw, uint32_t value) {
    if (fw->dbuf_rx_size >= DBUF_RX_MAX)
        return;

    int tail = (fw->dbuf_rx_head + fw->dbuf_rx_size) % DBUF_RX_MAX;

    fw->dbuf_rx[tail] = value;
    fw->dbuf_rx_size++;

    update_dbuf_rx_level(fw);
}

static uint32_t pop_dbuf_rx(Fw* fw) {
    if (!fw->dbuf_rx_size)
        return 0;

    uint32_t value = fw->dbuf_rx[fw->dbuf_rx_head];

    fw->dbuf_rx_head = (fw->dbuf_rx_head + 1) % DBUF_RX_MAX;
    fw->dbuf_rx_size--;

    update_dbuf_rx_level(fw);

    return value;
}

static uint32_t build_self_id_quad(uint32_t phy_id, uint32_t port0, uint32_t port1, uint32_t port2) {
    return 0x80000000u | (phy_id << 24) | (1u << 22) | (0x3fu << 16) | (2u << 14) | (4u << 8) |
        (port0 << 6) | (port1 << 4) | (port2 << 2);
}

static void trigger_bus_reset(Fw* fw) {
    fw->phy_r[0x00] = (uint8_t)((LOCAL_PHY_ID << 2) | 0x03);

    fw->reg[NODE_ID >> 2] = NODE_ID_RESET;

    fw->ctrl0 = (fw->ctrl0 | CTRL0_ROOT) & ~CTRL0_BUS_ID_RST;

    fw->dbuf_rx_head = 0;
    fw->dbuf_rx_size = 0;

    queue_dbuf_rx(fw, PHY_SELF_ID_PACKET);

    if (has_remote_node(fw)) {
        queue_dbuf_rx(fw, build_self_id_quad(REMOTE_PHY_ID, SELF_ID_PORT_PARENT, 0, 0));
        queue_dbuf_rx(fw, build_self_id_quad(LOCAL_PHY_ID, SELF_ID_PORT_CHILD, SELF_ID_PORT_NOT_CONNECTED, SELF_ID_PORT_NOT_CONNECTED));
    } else {
        queue_dbuf_rx(fw, build_self_id_quad(LOCAL_PHY_ID, SELF_ID_PORT_NOT_CONNECTED, SELF_ID_PORT_NOT_CONNECTED, SELF_ID_PORT_NOT_CONNECTED));
    }

    queue_dbuf_rx(fw, 1);

    for (int i = 0; i < NUM_PORTS; i++) {
        device::reset(&fw->device[i]);
    }

    raise_intr0(fw, INTR0_PHY_RST | INTR0_DRFR);

    iris_debug(fw, "Bus reset, local phy {} remote phy {}", LOCAL_PHY_ID, REMOTE_PHY_ID);
}

Fw* create(logger::Logger* logger, iop::intc::Intc* intc, iop::bus::Bus* bus, scheduler::Scheduler* sched) {
    Fw* fw = new Fw();

    fw->logger = logger;
    fw->logger_id = logger::register_source(logger, "fw");

    fw->hw.intc = intc;
    fw->hw.bus = bus;
    fw->hw.sched = sched;

    fw->reg[NODE_ID >> 2] = NODE_ID_RESET;

    init_phy(fw);

    return fw;
}

void reset(Fw* fw) {
    auto hw = fw->hw;

    logger::Logger* logger = fw->logger;
    size_t logger_id = fw->logger_id;

    device::Device device[NUM_PORTS];
    int device_type[NUM_PORTS];

    memcpy(device, fw->device, sizeof(device));
    memcpy(device_type, fw->device_type, sizeof(device_type));

    new (fw) Fw();

    fw->logger = logger;
    fw->logger_id = logger_id;

    fw->hw = hw;

    memcpy(fw->device, device, sizeof(device));
    memcpy(fw->device_type, device_type, sizeof(device_type));

    fw->reg[NODE_ID >> 2] = NODE_ID_RESET;

    init_phy(fw);

    for (int i = 0; i < NUM_PORTS; i++) {
        device::reset(&fw->device[i]);
    }
}

void destroy(Fw* fw) {
    for (int i = 0; i < NUM_PORTS; i++) {
        device::free(&fw->device[i]);
    }

    delete fw;
}

void fw_read_phy(Fw* fw) {
    uint8_t reg = (fw->phy_access >> 24) & 0xF;

    fw->phy_access &= ~0x80000000;

    uint8_t value = fw->phy_r[reg];

    if (((fw->phy_r[0x07] >> 5) & 0x7) == 0) {
        uint8_t port = fw->phy_r[0x07] & 0xf;

        if (reg == 0x08)
            value = (port == 0 && has_remote_node(fw)) ? 0xae : 0x00;

        if (reg == 0x09)
            value = (port == 0 && has_remote_node(fw)) ? 0x40 : 0x00;
    }

    fw->phy_access |= value | ((uint16_t)reg << 8);

    if (fw->intr0mask & 0x40000000) {
        fw->intr0 |= 0x40000000;

        iop::intc::irq(fw->hw.intc, iop::intc::FWRE);
    }

    iris_debug(fw, "PHY read from reg {} ({:08x})", reg, fw->phy_access & 0xff);
}

void fw_write_phy(Fw* fw) {
    uint8_t reg = (fw->phy_access >> 8) & 0xF;
    uint8_t value = fw->phy_access & 0xFF;

    if (reg != 0x00 && reg != 0x02 && reg != 0x03)
        fw->phy_r[reg] = value;

    fw->phy_access &= ~0x4000ffff;

    if (reg == 0x01 && (value & PHY_REG01_IBR)) {
        fw->phy_r[reg] &= ~PHY_REG01_IBR;

        trigger_bus_reset(fw);
    } else if (reg == 0x05 && (value & PHY_REG05_ISBR)) {
        fw->phy_r[reg] = (fw->phy_r[reg] & ~PHY_REG05_ISBR) | PHY_REG05_EN_ACCL | PHY_REG05_EN_MULTI;

        trigger_bus_reset(fw);
    }

    iris_debug(fw, "PHY write to reg {} ({:08x})", reg, value);
}

uint64_t read32(Fw* fw, uint32_t addr) {
    uint32_t reg = addr & 0x1ff;

    switch (reg) {
        case 0x8: return fw->ctrl0;
        case 0x10: return fw->ctrl2;
        case 0x14: return fw->phy_access;
        case 0x20: return fw->intr0;
        case 0x24: return fw->intr0mask;
        case 0x28: return fw->intr1;
        case 0x2C: return fw->intr1mask;
        case 0x30: return fw->intr2;
        case 0x34: return fw->intr2mask;
        case REG_7C: return REG_7C_VALUE;
        case DBUF_RX_DATA: return pop_dbuf_rx(fw);
    }

    return fw->reg[reg >> 2];
}

void write32(Fw* fw, uint32_t addr, uint64_t data) {
    uint32_t reg = addr & 0x1ff;

    switch (reg) {
        case 0x8: {
            fw->ctrl0 = data;
            fw->ctrl0 &= ~0x3800000;
        } return;
        case 0x10: {
            if (data & 0x2) //Power On
                fw->ctrl2 |= 0x8; //SCLK OK
        } return;
        case 0x14: {
            fw->phy_access = data;

            if (fw->phy_access & 0x40000000) {
                fw_write_phy(fw);
            } else if (fw->phy_access & 0x80000000) {
                fw_read_phy(fw);
            }
        } return;
        case 0x20: {
            fw->intr0 &= ~data;
        } return;
        case 0x24: {
            fw->intr0mask = data;
        } return;
        case 0x28: {
            fw->intr1 &= ~data;
        } return;
        case 0x2C: {
            fw->intr1mask = data;
        } return;
        case 0x30: {
            fw->intr2 &= ~data;
        } return;
        case 0x34: {
            fw->intr2mask = data;
        } return;
        case 0xB8: {
            fw->dma_ctrl_sr0 = data;

            iris_debug(fw, "DMA context 0 control {:08x}", (uint32_t)data);
        } return;
        case 0x138: {
            fw->dma_ctrl_sr1 = data;

            iris_debug(fw, "DMA context 1 control {:08x}", (uint32_t)data);
        } return;
    }

    fw->reg[reg >> 2] = (uint32_t)data;
}

}
