#pragma once

#include "queue.hpp"
#include "intc.hpp"
#include "scheduler.hpp"
#include "dma.hpp"
#include "logger.hpp"

namespace iris::sio2 {

inline constexpr auto DEV_PAD = 0x01;
inline constexpr auto DEV_PS1PAD = 0x42;// ?
inline constexpr auto DEV_MTAP = 0x21;
inline constexpr auto DEV_IR = 0x61;
inline constexpr auto DEV_MCD = 0x81;

struct Sio2;
struct Device {
    void (*handle_command)(Sio2*, void*, int);
    void (*detach)(void*);
    void* udata;
};

struct Sio2 {
    // Wiring. Set by create/connect, preserved across reset.
    struct {
        iop::dma::Dma* dma;
        iop::intc::Intc* intc;
        scheduler::Scheduler* sched;
    } hw;

    Device port[4];

    uint32_t ctrl;
    uint32_t send3[16];
    uint32_t send1[8];
    uint32_t send2[8];
    queue::Queue* in;
    queue::Queue* out;
    uint32_t recv1;
    uint32_t recv2;
    uint32_t recv3;
    uint32_t istat;

    int send3_index;


    logger::Logger* logger = nullptr;
    size_t logger_id = 0;
};

Sio2* create(logger::Logger* logger, iop::intc::Intc* intc, scheduler::Scheduler* sched);
void connect(Sio2* sio2, iop::dma::Dma* dma);
void destroy(Sio2* sio2);
uint64_t read8(Sio2* sio2, uint32_t addr);
uint64_t read32(Sio2* sio2, uint32_t addr);
void write8(Sio2* sio2, uint32_t addr, uint64_t data);
void write32(Sio2* sio2, uint32_t addr, uint64_t data);
void attach_device(Sio2* sio2, Device dev, int port);
void detach_device(Sio2* sio2, int port);
void dma_reset(Sio2* sio2);

}
