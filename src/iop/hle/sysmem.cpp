#include "../iop_def.hpp"

#include "sysmem.hpp"

namespace iris::iop::hle::sysmem {

#define SM_PUTCHAR(c) \
    iop->sm_putchar(iop->sm_putchar_udata, c);

inline constexpr auto REGISTER_PARAMS = 3;

static uint32_t fetch_next_param(iop::Iop* iop, int* index) {
    int param = (*index)++;

    if (param < REGISTER_PARAMS)
        return iop->r[5 + param];

    return iop::read32(iop, iop->r[29] + 16 + ((param - REGISTER_PARAMS) * 4));
}

int kprintf(iop::Iop* iop) {
    if (!iop->sm_putchar)
        return 0;

    int ptr = iop->r[4];
    int param = 0;

    char c = iop::read8(iop, ptr++);

    while (c != 0) {
        switch (c) {
            case '%': {
                int zero_pad = 0;
                int digits = 0;

                parse:

                c = iop::read8(iop, ptr++);

                switch (c) {
                    case 'c': {
                        char ch = fetch_next_param(iop, &param) & 0xff;

                        SM_PUTCHAR(ch);
                    } break;

                    case 's': {
                        uint32_t str_addr = fetch_next_param(iop, &param);
                        char ch = iop::read8(iop, str_addr++);

                        while (ch != 0) {
                            SM_PUTCHAR(ch);

                            ch = iop::read8(iop, str_addr++);
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
                        uint32_t val = fetch_next_param(iop, &param);

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

        c = iop::read8(iop, ptr++);
    }

    return 0;
}

}
