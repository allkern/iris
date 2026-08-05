#pragma once

#include "intc.hpp"
#include "scheduler.hpp"
#include "bus_decl.hpp"

#include "usb/device.hpp"
#include "logger.hpp"

namespace iris::usb {

inline constexpr auto OHCI_BASE = 0x1f801600;

// The PS2 OHCI root hub exposes 2 downstream ports
inline constexpr auto OHCI_NUM_PORTS = 2;

inline constexpr auto USB_HC_REVISION = 0x00;
inline constexpr auto USB_HC_CONTROL = 0x04;
inline constexpr auto USB_HC_COMMANDSTATUS = 0x08;
inline constexpr auto USB_HC_INTERRUPTSTATUS = 0x0c;
inline constexpr auto USB_HC_INTERRUPTENABLE = 0x10;
inline constexpr auto USB_HC_INTERRUPTDISABLE = 0x14;
inline constexpr auto USB_HC_HCCA = 0x18;
inline constexpr auto USB_HC_PERIODCURRENTED = 0x1c;
inline constexpr auto USB_HC_CONTROLHEADED = 0x20;
inline constexpr auto USB_HC_CONTROLCURRENTED = 0x24;
inline constexpr auto USB_HC_BULKHEADED = 0x28;
inline constexpr auto USB_HC_BULKCURRENTED = 0x2c;
inline constexpr auto USB_HC_DONEHEAD = 0x30;
inline constexpr auto USB_HC_FMINTERVAL = 0x34;
inline constexpr auto USB_HC_FMREMAINING = 0x38;
inline constexpr auto USB_HC_FMNUMBER = 0x3c;
inline constexpr auto USB_HC_PERIODICSTART = 0x40;
inline constexpr auto USB_HC_LSTHRESHOLD = 0x44;
inline constexpr auto USB_HC_RHDESCRIPTORA = 0x48;
inline constexpr auto USB_HC_RHDESCRIPTORB = 0x4c;
inline constexpr auto USB_HC_RHSTATUS = 0x50;
inline constexpr auto USB_HC_RHPORTSTATUS = 0x54;

enum {
    USB_DEVICE_NONE = 0,
    USB_DEVICE_KEYBOARD,
    USB_DEVICE_MOUSE,
    USB_DEVICE_MSD,
    USB_DEVICE_TYPE_COUNT
};

inline constexpr auto USB_MSD_PATH_MAX = 512;

struct Usb {
    // Wiring. Set by create/connect, preserved across reset.
    struct {
        iop::intc::Intc* intc;
        iop::bus::Bus* bus;
        scheduler::Scheduler* sched;
    } hw;

    // OHCI operational registers
    uint32_t hc_control;
    uint32_t hc_command_status;
    uint32_t hc_interrupt_status;
    uint32_t hc_interrupt_enable;
    uint32_t hc_hcca;
    uint32_t hc_period_current_ed;
    uint32_t hc_control_head_ed;
    uint32_t hc_control_current_ed;
    uint32_t hc_bulk_head_ed;
    uint32_t hc_bulk_current_ed;
    uint32_t hc_done_head;
    uint32_t hc_fm_interval;
    uint32_t hc_fm_remaining;
    uint32_t hc_fm_number;
    uint32_t hc_periodic_start;
    uint32_t hc_ls_threshold;
    uint32_t hc_rh_descriptor_a;
    uint32_t hc_rh_descriptor_b;
    uint32_t hc_rh_status;
    uint32_t hc_rh_port_status[OHCI_NUM_PORTS];

    uint32_t done_queue;

    device::Device device[OHCI_NUM_PORTS];

    int device_type[OHCI_NUM_PORTS];
    char msd_path[OHCI_NUM_PORTS][USB_MSD_PATH_MAX];
    int configured;

    int frame_scheduled;


    logger::Logger* logger = nullptr;
    size_t logger_id = 0;
};

Usb* create(logger::Logger* logger, iop::intc::Intc* intc, iop::bus::Bus* bus, scheduler::Scheduler* sched);
void reset(Usb* usb);
void destroy(Usb* usb);
uint64_t read32(Usb* usb, uint32_t addr);
void write32(Usb* usb, uint32_t addr, uint64_t data);
const char* device_type_name(int type);
int get_port_device(Usb* usb, int port);
void set_port_device(Usb* usb, int port, int type);
void msd_set_image(Usb* usb, int port, const char* path);
void kbd_key(Usb* usb, uint8_t usage, int pressed);
void mouse_move(Usb* usb, int dx, int dy, int dz);
void mouse_button(Usb* usb, int button, int pressed);

}
