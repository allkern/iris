#include "sysmem.h"

#define SM_PUTCHAR(c) \
    iop->sm_putchar(iop->sm_putchar_udata, c);

int reg_index = 0;

uint32_t fetch_next_param(struct iop_state* iop) {
    return iop->r[5 + reg_index++];
}

int sysmem_kprintf(struct iop_state* iop) {
    if (!iop->sm_putchar)
        return 0;

    int ptr = iop->r[4];

    reg_index = 5;

    char c = iop_read8(iop, ptr++);

    while (c != 0) {
        switch (c) {
            case '%': {
                int zero_pad = 0;
                int digits = 0;

                parse:

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

                    case '0': {
                        zero_pad = 1;

                        goto parse;
                    } break;

                    case '1': case '2': case '3': case '4': 
                    case '5': case '6': case '7': case '8':
                    case '9': {
                        digits = (digits * 10) + (c - '0');

                        goto parse;
                    } break;

                    case 'd': case 'u': case 'i': case 'x': case 'X':{
                        uint32_t val = fetch_next_param(iop);

                        char fmt_buf[8];
                        char* fmt = fmt_buf;

                        *fmt++ = '%';

                        if (zero_pad) {
                            *fmt++ = '0';
                        }

                        if (digits) {
                            fmt += sprintf(fmt, "%d", digits);
                        }

                        *fmt++ = c;
                        *fmt = '\0';

                        char buf[16];
                        sprintf(buf, fmt_buf, val);

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