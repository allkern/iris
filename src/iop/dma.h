#ifndef IOP_DMA_H
#define IOP_DMA_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include "u128.h"

#include "shared/sif.h"

#include "intc.h"
#include "cdvd.h"
#include "sched.h"
#include "sio2.h"
#include "spu2.h"

#include "bus_decl.h"

#include "ee/dmac.h"

#define IOP_DMA_MDEC_IN  0
#define IOP_DMA_MDEC_OUT 1
#define IOP_DMA_SIF2     2
#define IOP_DMA_CDVD     3
#define IOP_DMA_SPU1     4
#define IOP_DMA_PIO      5
#define IOP_DMA_OTC      6
#define IOP_DMA_SPU2     7
#define IOP_DMA_DEV9     8
#define IOP_DMA_SIF0     9
#define IOP_DMA_SIF1     10
#define IOP_DMA_SIO2_IN  11
#define IOP_DMA_SIO2_OUT 12

struct iop_dma_channel {
    uint32_t madr;
    uint32_t bcr;
    uint32_t chcr;
    uint32_t tadr;
    int transfer_pending;

    // Tag
    uint64_t tag;
    uint32_t addr;
    uint32_t size;
    int irq;
    int eot;
    int extra;
    uint32_t transfer_size;
};

struct ps2_iop_dma {
    struct iop_bus* bus;

    struct iop_dma_channel mdec_in;
    struct iop_dma_channel mdec_out;
    struct iop_dma_channel sif2;
    struct iop_dma_channel cdvd;
    struct iop_dma_channel spu1;
    struct iop_dma_channel pio;
    struct iop_dma_channel otc;
    struct iop_dma_channel spu2;
    struct iop_dma_channel dev9;
    struct iop_dma_channel sif0;
    struct iop_dma_channel sif1;
    struct iop_dma_channel sio2_in;
    struct iop_dma_channel sio2_out;

    uint32_t dpcr;
    uint32_t dpcr2;
    uint32_t dicr;
    uint32_t dicr2;
    uint32_t dmacen;
    uint32_t dmacinten;

    struct ps2_iop_intc* intc;
    struct ps2_sif* sif;
    struct ps2_cdvd* drive;
    struct ps2_dmac* ee_dma;
    struct ps2_sio2* sio2;
    struct ps2_spu2* spu;
    struct sched_state* sched;
};

struct ps2_iop_dma* ps2_iop_dma_create(void);
void ps2_iop_dma_init(struct ps2_iop_dma* dma, struct ps2_iop_intc* intc, struct ps2_sif* sif, struct ps2_cdvd* cdvd, struct ps2_dmac* ee_dma, struct ps2_sio2* sio2, struct ps2_spu2* spu, struct sched_state* sched, struct iop_bus* bus);
void ps2_iop_dma_destroy(struct ps2_iop_dma* dma);
uint64_t ps2_iop_dma_read16(struct ps2_iop_dma* dma, uint32_t addr);
void ps2_iop_dma_write16(struct ps2_iop_dma* dma, uint32_t addr, uint64_t data);
uint64_t ps2_iop_dma_read32(struct ps2_iop_dma* dma, uint32_t addr);
void ps2_iop_dma_write32(struct ps2_iop_dma* dma, uint32_t addr, uint64_t data);
void iop_dma_handle_mdec_in_transfer(struct ps2_iop_dma* dma);
void iop_dma_handle_mdec_out_transfer(struct ps2_iop_dma* dma);
void iop_dma_handle_sif2_transfer(struct ps2_iop_dma* dma);
void iop_dma_handle_cdvd_transfer(struct ps2_iop_dma* dma);
void iop_dma_handle_spu1_transfer(struct ps2_iop_dma* dma);
void iop_dma_handle_pio_transfer(struct ps2_iop_dma* dma);
void iop_dma_handle_otc_transfer(struct ps2_iop_dma* dma);
void iop_dma_handle_spu2_transfer(struct ps2_iop_dma* dma);
void iop_dma_handle_dev9_transfer(struct ps2_iop_dma* dma);
void iop_dma_handle_sif0_transfer(struct ps2_iop_dma* dma);
void iop_dma_handle_sif1_transfer(struct ps2_iop_dma* dma);
void iop_dma_handle_sio2_in_transfer(struct ps2_iop_dma* dma);
void iop_dma_handle_sio2_out_transfer(struct ps2_iop_dma* dma);

void iop_dma_end_spu1_transfer(struct ps2_iop_dma* dma);
void iop_dma_end_spu2_transfer(struct ps2_iop_dma* dma);

#ifdef __cplusplus
}
#endif

#endif