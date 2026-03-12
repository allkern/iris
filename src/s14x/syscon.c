#include "syscon.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

struct s14x_syscon* s14x_syscon_create(void) {
    return malloc(sizeof(struct s14x_syscon));
}

void s14x_syscon_init(struct s14x_syscon* syscon) {
    memset(syscon, 0, sizeof(struct s14x_syscon));
}

uint64_t s14x_syscon_read(struct s14x_syscon* syscon, uint32_t addr) {
    switch (addr) {
        case S14X_SYSCON_REG_LED: return syscon->led;
        case S14X_SYSCON_REG_SECURITY_UNLOCK: return syscon->security_unlock;
        case S14X_SYSCON_REG_RTC_FLAG: {
            switch (syscon->rtc_state) {
                case S14X_RTC_STATE_READ_YEAR: syscon->rtc_flag = 23; break;
                case S14X_RTC_STATE_READ_MONTH: syscon->rtc_flag = 6; break;
                case S14X_RTC_STATE_READ_DAY: syscon->rtc_flag = 15; break;
                case S14X_RTC_STATE_READ_DOW: syscon->rtc_flag = 4; break;
                case S14X_RTC_STATE_READ_HOURS: syscon->rtc_flag = 12; break;
                case S14X_RTC_STATE_READ_MINUTES: syscon->rtc_flag = 34; break;
                case S14X_RTC_STATE_READ_SECONDS: syscon->rtc_flag = 56; break;
            }

            int b = (syscon->rtc_flag >> syscon->rtc_bit++) & 1;
            int bits = syscon->rtc_state == S14X_RTC_STATE_READ_DOW ? 4 : 8;

            if (syscon->rtc_bit >= bits) {
                syscon->rtc_bit = 0;
                syscon->rtc_state++;

                if (syscon->rtc_state > S14X_RTC_STATE_READ_SECONDS) {
                    syscon->rtc_state = S14X_RTC_STATE_READ_YEAR;
                }
            }

            // printf("s14x_rtc: read RTC_FLAG %d\n", syscon->rtc_flag);

            return b;
        }
        case S14X_SYSCON_REG_WATCHDOG_FLAG2: return syscon->watchdog_flag2;
        case S14X_SYSCON_REG_SRAM_WRITE_FLAG: return syscon->sram_write_flag;
        case S14X_SYSCON_REG_SECURITY_UNLOCK_SET1: return syscon->security_unlock_set1;
        case S14X_SYSCON_REG_SECURITY_UNLOCK_SET2: return syscon->security_unlock_set2;
    }

    return 0;
}

void s14x_syscon_write(struct s14x_syscon* syscon, uint32_t addr, uint64_t data) {
    switch (addr) {
        case S14X_SYSCON_REG_LED: syscon->led = data; return;
        case S14X_SYSCON_REG_SECURITY_UNLOCK: syscon->security_unlock = data; return;
        // case S14X_SYSCON_REG_RTC_FLAG: syscon->rtc_flag = data; return;
        case S14X_SYSCON_REG_WATCHDOG_FLAG2: syscon->watchdog_flag2 = data; return;
        case S14X_SYSCON_REG_SRAM_WRITE_FLAG: syscon->sram_write_flag = data; return;
        case S14X_SYSCON_REG_SECURITY_UNLOCK_SET1: syscon->security_unlock_set1 = data; return;
        case S14X_SYSCON_REG_SECURITY_UNLOCK_SET2: syscon->security_unlock_set2 = data; return;
    }
}

void s14x_syscon_destroy(struct s14x_syscon* syscon) {
    free(syscon);
}