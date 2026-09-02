#include "iop/intc.hpp"

#include "sif.hpp"

namespace iris::sif {

Sif* create(logger::Logger* logger) {
    Sif* sif = new Sif();

    sif->logger = logger;
    sif->logger_id = logger::register_source(logger, "sif");

    return sif;
}

void connect(Sif* sif, iop::intc::Intc* iop_intc) {
    sif->ctrl = 0xf0000012;
    sif->iop_intc = iop_intc;
}

void destroy(Sif* sif) {
    delete sif;
}

uint64_t read32(Sif* sif, uint32_t addr) {
    switch (addr) {
        // IOP side
        case 0x1d000000: return sif->mscom;
        case 0x1d000010: return sif->smcom;
        case 0x1d000020: return sif->msflg;
        case 0x1d000030: return sif->smflg;
        case 0x1d000040: return sif->ctrl | 0xf0000001;
        case 0x1d000060: return sif->bd6;

        // EE side
        case 0x1000f200: return sif->mscom;
        case 0x1000f210: return sif->smcom;
        case 0x1000f220: return sif->msflg;
        case 0x1000f230: return sif->smflg;
        case 0x1000f240: return sif->ctrl | 0xf0000101;
        case 0x1000f260: return sif->bd6;
    }

    return 0;
}

void write32(Sif* sif, uint32_t addr, uint64_t data) {
    switch (addr) {
        // IOP side
        case 0x1d000000: sif->mscom = data; return;
        case 0x1d000010: sif->smcom = data; return;
        case 0x1d000020: sif->msflg &= ~data; return;
        case 0x1d000030: sif->smflg |= data; return;
        case 0x1d000040: sif->ctrl = data; return;
        case 0x1d000060: sif->bd6 = data; return;

        // EE side. Note msflg/smflg set and clear on opposite sides.
        case 0x1000f200: sif->mscom = data; return;
        case 0x1000f210: sif->smcom = data; return;
        case 0x1000f220: sif->msflg |= data; return;
        case 0x1000f230: sif->smflg &= ~data; return;
        case 0x1000f240: {
            if (data & 0x40000)
                iop::intc::irq(sif->iop_intc, iop::intc::SBUS);

            sif->ctrl = data & ~0x40000;
        } return;
        case 0x1000f260: sif->bd6 = data; return;
    }
}

void fifo_write(Fifo& fifo, uint128_t data) {
    if ((size_t)fifo.write_index >= fifo.data.size())
        fifo.data.resize(fifo.write_index + 1);

    fifo.data[fifo.write_index++] = data;
}

uint128_t fifo_read(Fifo& fifo) {
    // If the EE asks for more than the IOP produced, repeat the last quadword
    // actually transferred. This happens during SIF init: the IOP starts a
    // transfer whose EE tag requests 2 QW, but the IOP only ever wrote 2 QW.
    if (fifo.read_index == fifo.write_index)
        return fifo.read_index ? fifo.data[fifo.read_index - 1] : uint128_t{};

    return fifo.data[fifo.read_index++];
}

void fifo_reset(Fifo& fifo) {
    fifo.read_index = 0;
    fifo.write_index = 0;
}

void reset(Sif* sif) {
    sif->mscom = 0;
    sif->smcom = 0;
    sif->msflg = 0;
    sif->smflg = 0;
    sif->ctrl = 0;
    sif->bd6 = 0;

    fifo_reset(sif->sif0);
    fifo_reset(sif->sif1);
}

bool fifo_is_empty(const Fifo& fifo) {
    return fifo.read_index == fifo.write_index;
}

}
