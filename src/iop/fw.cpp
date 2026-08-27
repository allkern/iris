#include <new>
#include <cstring>

#include "fw.hpp"
#include "kp1/p1io.hpp"

namespace iris::fw {

static void trigger_bus_reset(Fw* fw);

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

    dev->host = fw;

    if (fw_device_types[type].create) {
        fw_device_types[type].create(dev);

        iris_info(fw, "port {} device set to {}", port, fw_device_types[type].name);
    }

    trigger_bus_reset(fw);
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

static void flush_pending_rx(Fw* fw);

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
    fw->pending_head = 0;
    fw->pending_count = 0;
    fw->ubuf_rx_head = 0;
    fw->ubuf_rx_size = 0;

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

    iris_debug(fw, "bus reset");
}

static void flush_pending_rx(Fw* fw) {
    if (!fw->pending_count || fw->dbuf_rx_size)
        return;

    int index = fw->pending_head;

    for (int i = 0; i < fw->pending_size[index]; i++) {
        queue_dbuf_rx(fw, fw->pending[index][i]);
    }

    fw->pending_head = (fw->pending_head + 1) % PENDING_PACKETS;
    fw->pending_count--;

    raise_intr0(fw, INTR0_URX);
}

static uint32_t* alloc_pending(Fw* fw, int quads) {
    if (fw->pending_count >= PENDING_PACKETS || quads > PENDING_QUADS)
        return NULL;

    int index = (fw->pending_head + fw->pending_count) % PENDING_PACKETS;

    fw->pending_size[index] = quads;
    fw->pending_count++;

    return fw->pending[index];
}

int write_iop_memory(Fw* fw, uint32_t address, const uint8_t* data, uint32_t size) {
    for (uint32_t i = 0; i < size; i++) {
        iop::bus::write8(fw->hw.bus, address + i, data[i]);
    }

    return 1;
}

int read_iop_memory(Fw* fw, uint32_t address, uint8_t* data, uint32_t size) {
    for (uint32_t i = 0; i < size; i++) {
        data[i] = (uint8_t)iop::bus::read8(fw->hw.bus, address + i);
    }

    return 1;
}

void queue_remote_write_quad(Fw* fw, uint32_t offset_high, uint32_t offset_low, uint32_t payload) {
    uint32_t* packet = alloc_pending(fw, 5);

    if (!packet)
        return;

    packet[0] = (0x3ffu << 22) | (RESPONSE_SPEED << 16) | (1u << 10) | (TCODE_WRITEQ << 4);
    packet[1] = (LOCAL_NODE_ID << 16) | offset_high;
    packet[2] = offset_low;
    packet[3] = payload;
    packet[4] = 2;

    flush_pending_rx(fw);
}

void queue_remote_write_quads(Fw* fw, uint32_t offset_high, uint32_t offset_low, const uint32_t* payload, uint32_t count) {
    uint32_t* packet = alloc_pending(fw, (int)count + 5);

    if (!packet)
        return;

    if (offset_high == OFFSET_HIGH_IOP_DMA && count) {
        for (uint32_t i = 0; i < count; i++) {
            uint8_t quad[4];

            quad[0] = (uint8_t)payload[i];
            quad[1] = (uint8_t)(payload[i] >> 8);
            quad[2] = (uint8_t)(payload[i] >> 16);
            quad[3] = (uint8_t)(payload[i] >> 24);

            write_iop_memory(fw, offset_low + (i * 4), quad, 4);
        }
    }

    packet[0] = (0x3ffu << 22) | (RESPONSE_SPEED << 16) | (TCODE_WRITEB << 4);
    packet[1] = (LOCAL_NODE_ID << 16) | offset_high;
    packet[2] = offset_low;
    packet[3] = (count * 4) << 16;

    for (uint32_t i = 0; i < count; i++) {
        packet[4 + i] = payload[i];
    }

    packet[count + 4] = 2;

    flush_pending_rx(fw);
}

void queue_remote_write_bytes(Fw* fw, uint32_t offset_high, uint32_t offset_low, const uint8_t* payload, uint32_t count) {
    int quads = (int)((count + 3) >> 2);

    uint32_t* packet = alloc_pending(fw, quads + 5);

    if (!packet)
        return;

    if (offset_high == OFFSET_HIGH_IOP_DMA && count)
        write_iop_memory(fw, offset_low, payload, count);

    packet[0] = (0x3ffu << 22) | (RESPONSE_SPEED << 16) | (TCODE_WRITEB << 4);
    packet[1] = (LOCAL_NODE_ID << 16) | offset_high;
    packet[2] = offset_low;
    packet[3] = count << 16;

    for (int i = 0; i < quads; i++) {
        uint32_t value = 0;

        for (uint32_t j = 0; j < 4; j++) {
            uint32_t index = (uint32_t)(i * 4) + j;

            if (index < count)
                value |= (uint32_t)payload[index] << (24 - (j * 8));
        }

        packet[4 + i] = value;
    }

    packet[quads + 4] = 2;

    flush_pending_rx(fw);
}

static void raise_intr1(Fw* fw, uint32_t bits) {
    fw->intr1 |= bits;

    if (fw->intr1mask & bits)
        iop::intc::irq(fw->hw.intc, iop::intc::FWRE);
}

static void update_ubuf_rx_level(Fw* fw) {
    fw->reg[UBUF_RX_LVL >> 2] = (uint32_t)fw->ubuf_rx_size;
}

static void queue_ubuf_rx(Fw* fw, uint32_t value) {
    if (fw->ubuf_rx_size >= UBUF_RX_MAX)
        return;

    int tail = (fw->ubuf_rx_head + fw->ubuf_rx_size) % UBUF_RX_MAX;

    fw->ubuf_rx[tail] = value;
    fw->ubuf_rx_size++;

    update_ubuf_rx_level(fw);
}

static uint32_t pop_ubuf_rx(Fw* fw) {
    if (!fw->ubuf_rx_size)
        return 0;

    uint32_t value = fw->ubuf_rx[fw->ubuf_rx_head];

    fw->ubuf_rx_head = (fw->ubuf_rx_head + 1) % UBUF_RX_MAX;
    fw->ubuf_rx_size--;

    update_ubuf_rx_level(fw);

    return value;
}

static device::Device* active_device(Fw* fw) {
    for (int i = 0; i < NUM_PORTS; i++) {
        if (fw->device[i].connected)
            return &fw->device[i];
    }

    return NULL;
}

static void put_be32(uint8_t* buf, uint32_t value) {
    buf[0] = (uint8_t)(value >> 24);
    buf[1] = (uint8_t)(value >> 16);
    buf[2] = (uint8_t)(value >> 8);
    buf[3] = (uint8_t)value;
}

static uint32_t get_be32(const uint8_t* buf) {
    return ((uint32_t)buf[0] << 24) | ((uint32_t)buf[1] << 16) | ((uint32_t)buf[2] << 8) | buf[3];
}

static void ack_transmit(Fw* fw, uint32_t ack) {
    fw->reg[ACK_STAT >> 2] = ack << 28;

    raise_intr0(fw, INTR0_ACK_RCVD);
}

static void queue_read_response(Fw* fw, uint32_t request_header, uint16_t dest_node, uint32_t value) {
    uint32_t tlabel = (request_header >> 10) & 0x3f;
    uint32_t bus_id = dest_node >> 6;

    queue_ubuf_rx(fw, (bus_id << 22) | (RESPONSE_SPEED << 16) | (tlabel << 10) | (1u << 8) | (TCODE_READQ_RESPONSE << 4));
    queue_ubuf_rx(fw, (uint32_t)dest_node << 16);
    queue_ubuf_rx(fw, 0);
    queue_ubuf_rx(fw, value);
    queue_ubuf_rx(fw, 1);

    raise_intr0(fw, INTR0_URX);
}

static void process_ubuf_tx(Fw* fw) {
    if (!fw->ubuf_tx_size)
        return;

    uint32_t header = fw->ubuf_tx[0];
    uint32_t tcode = (header >> 4) & 0xf;

    device::Device* dev = active_device(fw);

    if ((tcode == TCODE_WRITEQ || tcode == TCODE_WRITEB) && fw->ubuf_tx_size >= 4) {
        uint64_t offset = ((uint64_t)(fw->ubuf_tx[1] & 0xffff) << 32) | fw->ubuf_tx[2];

        int payload_start = tcode == TCODE_WRITEQ ? 3 : 4;
        int payload_quads = fw->ubuf_tx_size > payload_start ? fw->ubuf_tx_size - payload_start : 0;

        if (payload_quads > 0) {
            uint8_t payload[UBUF_TX_MAX * 4];

            for (int i = 0; i < payload_quads; i++) {
                put_be32(payload + (i * 4), fw->ubuf_tx[payload_start + i]);
            }

            int result = dev ? device::write(dev, offset, payload, payload_quads * 4) : device::RESP_ADDRESS_ERROR;

            if (result == device::RESP_COMPLETE)
                raise_intr0(fw, INTR0_PB_CNT_R);

            // iris_debug(fw, "UBUF write off {:012x} quads {} result {}", offset, payload_quads, result);
        }

        ack_transmit(fw, ACK_COMPLETE);
    } else if ((tcode == TCODE_READQ || tcode == TCODE_READB) && fw->ubuf_tx_size >= 3) {
        uint16_t node = (uint16_t)(fw->ubuf_tx[1] >> 16);
        uint64_t offset = ((uint64_t)(fw->ubuf_tx[1] & 0xffff) << 32) | fw->ubuf_tx[2];

        uint8_t quad[4] = { 0, 0, 0, 0 };

        int result = dev ? device::read(dev, offset, quad, 4) : device::RESP_ADDRESS_ERROR;

        uint32_t value = result == device::RESP_COMPLETE ? get_be32(quad) : 0;

        // iris_debug(fw, "UBUF read off {:012x} value {:08x} result {}", offset, value, result);

        ack_transmit(fw, ACK_PEND);

        queue_read_response(fw, header, node, value);
    }

    raise_intr1(fw, INTR1_UTD);

    fw->ubuf_tx_size = 0;

    flush_pending_rx(fw);
}

static void try_process_pht_write(Fw* fw, int channel) {
    if (!fw->pht_write_pending[channel])
        return;

    uint32_t expected = fw->pht_tx_expected[channel];

    if (!expected || (uint32_t)(fw->pht_tx_size[channel] * 4) < expected)
        return;

    uint32_t base = channel == 0 ? PHT_CTRL0 : PHT_CTRL1;

    uint32_t hdr0 = fw->reg[(base + 0x08) >> 2];
    uint32_t hdr1 = fw->reg[(base + 0x0c) >> 2];

    uint64_t offset = ((uint64_t)(hdr0 & 0xffff) << 32) | hdr1;

    int payload_quads = (int)((expected + 3) >> 2);

    static uint8_t payload[PHT_TX_MAX * 4];

    for (int i = 0; i < payload_quads; i++) {
        uint32_t value = fw->pht_tx[channel][i];

        payload[(i * 4) + 0] = (uint8_t)value;
        payload[(i * 4) + 1] = (uint8_t)(value >> 8);
        payload[(i * 4) + 2] = (uint8_t)(value >> 16);
        payload[(i * 4) + 3] = (uint8_t)(value >> 24);
    }

    device::Device* dev = active_device(fw);

    int result = dev ? device::write(dev, offset, payload, (int)expected) : device::RESP_ADDRESS_ERROR;

    // iris_debug(fw, "PHT{} write off {:012x} bytes {:x} result {}", channel, offset, expected, result);

    fw->pht_write_pending[channel] = 0;
    fw->pht_tx_expected[channel] = 0;
    fw->pht_tx_size[channel] = 0;

    fw->reg[(base + 0x24) >> 2] = 0;

    raise_intr0(fw, INTR0_PB_CNT_R);
}

static void begin_pht_request(Fw* fw, int channel, uint32_t control) {
    uint32_t base = channel == 0 ? PHT_CTRL0 : PHT_CTRL1;

    if (!(control & PHT_CTRL_EWREQ)) {
        fw->reg[(base + 0x24) >> 2] = 0;

        raise_intr0(fw, INTR0_PB_CNT_R);

        return;
    }

    fw->pht_write_pending[channel] = 1;
    fw->pht_tx_expected[channel] = fw->reg[(base + 0x10) >> 2] & 0xffff;
    fw->pht_tx_size[channel] = 0;

    try_process_pht_write(fw, channel);
}

static void push_pht_tx(Fw* fw, int channel, uint32_t value) {
    if (fw->pht_tx_size[channel] < PHT_TX_MAX)
        fw->pht_tx[channel][fw->pht_tx_size[channel]++] = value;

    uint32_t level = channel == 0 ? DBUF_FIFO_LV0 : DBUF_FIFO_LV1;

    fw->reg[level >> 2] = (uint32_t)fw->pht_tx_size[channel] << 16;

    try_process_pht_write(fw, channel);
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

    trigger_bus_reset(fw);
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
    uint8_t reg = (fw->phy_access >> 24) & 0xF;
    uint8_t value = (fw->phy_access >> 16) & 0xFF;

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

static uint64_t read32_inner(Fw* fw, uint32_t reg) {
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
        case DBUF_RX_DATA: {
            uint32_t value = pop_dbuf_rx(fw);

            flush_pending_rx(fw);

            return value;
        }
        case UBUF_RX: return pop_ubuf_rx(fw);
    }

    return fw->reg[reg >> 2];
}

uint64_t read32(Fw* fw, uint32_t addr) {
    uint32_t reg = addr & 0x1ff;

    uint64_t value = read32_inner(fw, reg);

    // iris_debug(fw, "r {:03x} -> {:08x}", reg, (uint32_t)value);

    return value;
}

static void write32_inner(Fw* fw, uint32_t reg, uint64_t data) {
    switch (reg) {
        case NODE_ID: {
            fw->reg[reg >> 2] = NODE_ID_RESET;
        } return;
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
        case INTR0_MASK: {
            fw->intr0mask = (uint32_t)data;

            if (fw->intr0mask)
                fw->intr0mask |= INTR0_PHY_RST;

            if (fw->intr0 & fw->intr0mask)
                iop::intc::irq(fw->hw.intc, iop::intc::FWRE);
        } return;
        case 0x28: {
            fw->intr1 &= ~data;
        } return;
        case INTR1_MASK: {
            fw->intr1mask = (uint32_t)data;

            if (fw->intr1 & fw->intr1mask)
                iop::intc::irq(fw->hw.intc, iop::intc::FWRE);
        } return;
        case 0x30: {
            fw->intr2 &= ~data;
        } return;
        case 0x34: {
            fw->intr2mask = data;
        } return;
        case DMA_CTRL0: {
            fw->dma_ctrl_sr0 = (uint32_t)data;
        } break;
        case DMA_CTRL1: {
            fw->dma_ctrl_sr1 = (uint32_t)data;
        } break;

        case UBUF_TX_NEXT: {
            if (fw->ubuf_tx_size < UBUF_TX_MAX)
                fw->ubuf_tx[fw->ubuf_tx_size++] = (uint32_t)data;
        } break;
        case UBUF_TX_LAST: {
            if (fw->ubuf_tx_size < UBUF_TX_MAX)
                fw->ubuf_tx[fw->ubuf_tx_size++] = (uint32_t)data;

            process_ubuf_tx(fw);
        } break;
        case UBUF_TX_CLR: {
            fw->ubuf_tx_size = 0;
        } break;
        case UBUF_RX_CLR: {
            fw->ubuf_rx_head = 0;
            fw->ubuf_rx_size = 0;

            update_ubuf_rx_level(fw);
        } break;

        case DBUF_FIFO_LV0: {
            if (data & DBUF_FIFO_RESET_RX) {
                fw->dbuf_rx_head = 0;
                fw->dbuf_rx_size = 0;
            }

            update_dbuf_rx_level(fw);

            fw->reg[reg >> 2] |= (uint32_t)data & DBUF_FIFO_RESET_TX;
        } return;

        case PHT_CTRL0:
        case PHT_CTRL1: {
            fw->reg[reg >> 2] = (uint32_t)data & ~PHT_CTRL_RST;

            if (data & (PHT_CTRL_EWREQ | PHT_CTRL_ERREQ))
                begin_pht_request(fw, reg == PHT_CTRL0 ? 0 : 1, (uint32_t)data);
        } return;

        case DBUF_TX_DATA0: {
            push_pht_tx(fw, 0, (uint32_t)data);
        } break;
        case DBUF_TX_DATA1: {
            push_pht_tx(fw, 1, (uint32_t)data);
        } break;
    }

    fw->reg[reg >> 2] = (uint32_t)data;
}

void write32(Fw* fw, uint32_t addr, uint64_t data) {
    uint32_t reg = addr & 0x1ff;

    // iris_debug(fw, "w {:03x} <- {:08x}", reg, (uint32_t)data);

    write32_inner(fw, reg, data);
}

}
