#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "usb.h"

struct ps2_usb* ps2_usb_create(void) {
    return malloc(sizeof(struct ps2_usb));
}

void ps2_usb_init(struct ps2_usb* usb) {
    memset(usb, 0, sizeof(struct ps2_usb));
}

void ps2_usb_destroy(struct ps2_usb* usb) {
    free(usb);
}

uint64_t ps2_usb_read32(struct ps2_usb* usb, uint32_t addr) {
    addr &= 0xff;

    switch (addr) {
        case 0x00: printf("usb: read USB_HC_REVISION\n"); return 0;
        case 0x04: printf("usb: read USB_HC_CONTROL\n"); return 0;
        case 0x08: printf("usb: read USB_HC_COMMANDSTATUS\n"); return 0;
        case 0x0c: printf("usb: read USB_HC_INTERRUPTSTATUS\n"); return 0xffffffff;
        case 0x10: printf("usb: read USB_HC_INTERRUPTENABLE\n"); return 0xffffffff;
        case 0x14: printf("usb: read USB_HC_INTERRUPTDISABLE\n"); return 0;
        case 0x18: printf("usb: read USB_HC_HCCA\n"); return 0;
        case 0x1c: printf("usb: read USB_HC_PERIODCURRENTED\n"); return 0;
        case 0x20: printf("usb: read USB_HC_CONTROLHEADED\n"); return 0;
        case 0x24: printf("usb: read USB_HC_CONTROLCURRENTED\n"); return 0;
        case 0x28: printf("usb: read USB_HC_BULKHEADED\n"); return 0;
        case 0x2c: printf("usb: read USB_HC_BULKCURRENTED\n"); return 0;
        case 0x30: printf("usb: read USB_HC_DONEHEAD\n"); return 0;
        case 0x34: printf("usb: read USB_HC_FMINTERVAL\n"); return 0;
        case 0x38: printf("usb: read USB_HC_FMREMAINING\n"); return 0;
        case 0x3c: printf("usb: read USB_HC_FMNUMBER\n"); return 0;
        case 0x40: printf("usb: read USB_HC_PERIODICSTART\n"); return 0;
        case 0x44: printf("usb: read USB_HC_LSTHRESHOLD\n"); return 0;
        case 0x48: printf("usb: read USB_HC_RHDESCRIPTORA\n"); return 0x3F000202;
        case 0x4c: printf("usb: read USB_HC_RHDESCRIPTORB\n"); return 0xffff;
        case 0x50: printf("usb: read USB_HC_RHSTATUS\n"); return 0xffffffff;
    }

    if (addr >= 0x54 && addr <= 0x8c) {
        printf("usb: Read USB_HC_RHPORT%dSTATUS\n", (addr - 0x54) >> 2);
        return 0x150303;
    }

    printf("usb: Unhandled read at %08x\n", addr);

    exit(1);

    return 0;
}

void ps2_usb_write32(struct ps2_usb* usb, uint32_t addr, uint64_t data) {
    addr &= 0xff;

    switch (addr) {
        case 0x00: printf("usb: write %08x to USB_HC_REVISION\n", data); return;
        case 0x04: printf("usb: write %08x to USB_HC_CONTROL\n", data); return;
        case 0x08: printf("usb: write %08x to USB_HC_COMMANDSTATUS\n", data); return;
        case 0x0c: printf("usb: write %08x to USB_HC_INTERRUPTSTATUS\n", data); return;
        case 0x10: printf("usb: write %08x to USB_HC_INTERRUPTENABLE\n", data); return;
        case 0x14: printf("usb: write %08x to USB_HC_INTERRUPTDISABLE\n", data); return;
        case 0x18: printf("usb: write %08x to USB_HC_HCCA\n", data); return;
        case 0x1c: printf("usb: write %08x to USB_HC_PERIODCURRENTED\n", data); return;
        case 0x20: printf("usb: write %08x to USB_HC_CONTROLHEADED\n", data); return;
        case 0x24: printf("usb: write %08x to USB_HC_CONTROLCURRENTED\n", data); return;
        case 0x28: printf("usb: write %08x to USB_HC_BULKHEADED\n", data); return;
        case 0x2c: printf("usb: write %08x to USB_HC_BULKCURRENTED\n", data); return;
        case 0x30: printf("usb: write %08x to USB_HC_DONEHEAD\n", data); return;
        case 0x34: printf("usb: write %08x to USB_HC_FMINTERVAL\n", data); return;
        case 0x38: printf("usb: write %08x to USB_HC_FMREMAINING\n", data); return;
        case 0x3c: printf("usb: write %08x to USB_HC_FMNUMBER\n", data); return;
        case 0x40: printf("usb: write %08x to USB_HC_PERIODICSTART\n", data); return;
        case 0x44: printf("usb: write %08x to USB_HC_LSTHRESHOLD\n", data); return;
        case 0x48: printf("usb: write %08x to USB_HC_RHDESCRIPTORA\n", data); return;
        case 0x4c: printf("usb: write %08x to USB_HC_RHDESCRIPTORB\n", data); return;
        case 0x50: printf("usb: write %08x to USB_HC_RHSTATUS\n", data); return;
    }

    if (addr >= 0x54 && addr <= 0x8c) {
        printf("usb: write %08x to USB_HC_RHPORT%dSTATUS\n", data, (addr - 0x54) >> 2);

        return;
    }

    printf("usb: Unhandled write at %08x (%08x)\n", addr, data);

    exit(1);

    return;
}