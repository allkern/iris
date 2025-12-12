#include "sysmem.h"

#define SM_PUTCHAR(c) \
    iop->sm_putchar(iop->sm_putchar_udata, c);

int reg_index = 0;

uint32_t fetch_next_param(struct iop_state* iop) {
    return iop->r[5 + reg_index++];
}

int sysmem_kprintf(struct iop_state* iop) {
    int ptr = iop->r[4];

    reg_index = 5;

    char c = iop_read8(iop, ptr++);

    while (c != 0) {
        switch (c) {
            case '%': {
                c = iop_read8(iop, ptr++);

                switch (c) {
                    case 'c': {
                        char ch = fetch_next_param(iop) & 0xff;

                        SM_PUTCHAR(ch);
                    } break;

                    case 's': {
                        uint32_t str_addr = fetch_next_param(iop);
                        char ch = iop_read8(iop, str_addr++);

                        while (ch != 0) {
                            SM_PUTCHAR(ch);

                            ch = iop_read8(iop, str_addr++);
                        }
                    } break;

                    case 'd': {
                        int val = fetch_next_param(iop);
                        char buf[16];
                        sprintf(buf, "%d", val);

                        for (char* p = buf; *p != 0; p++) {
                            SM_PUTCHAR(*p);
                        }
                    } break;

                    case '%': {
                        SM_PUTCHAR('%');
                    } break;

                    default: {
                        printf("sysmem_kprintf: Unknown format specifier %c\n", c);
                        // Unknown format specifier, just print it as is
                        SM_PUTCHAR('%');
                        SM_PUTCHAR(c);
                    } break;
                }
            } break;

            default:
                SM_PUTCHAR(c);

                break;
        }

        c = iop_read8(iop, ptr++);
    }

    return 0;
}