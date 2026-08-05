#include <new>
#include "syscon.hpp"

namespace iris::s14x::syscon {

Syscon* create(logger::Logger* logger) {
    Syscon* syscon = new Syscon();

    syscon->logger = logger;
    syscon->logger_id = logger::register_source(logger, "syscon");

    syscon->battery_level = 0x0; // 0 - OK, non-zero - NG

    return syscon;
}

uint64_t read8(Syscon* syscon, uint32_t addr) {
    addr -= 0x10000000;

    switch (addr) {
        case REG_LED: return syscon->led;
        case REG_SECURITY_UNLOCK: return syscon->security_unlock;
        case REG_RTC_FLAG: {
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

            // iris_debug(syscon, "s14x_rtc: read RTC_FLAG {}", syscon->rtc_flag);

            return b;
        }
        case REG_WATCHDOG_FLAG2: return syscon->watchdog_flag2;
        case REG_BATTERY_LEVEL: return syscon->battery_level;
        case REG_SRAM_WRITE_FLAG: return syscon->sram_write_flag;
        case REG_SECURITY_UNLOCK_SET1: return syscon->security_unlock_set1;
        case REG_SECURITY_UNLOCK_SET2: return syscon->security_unlock_set2;
        default: iris_debug(syscon, "Syscon: Unknown register read {:08x}", addr); return 0;
    }

    return 0;
}

void write8(Syscon* syscon, uint32_t addr, uint64_t data) {
    addr -= 0x10000000;

    switch (addr) {
        case REG_LED: syscon->led = data; return;
        case REG_SECURITY_UNLOCK: syscon->security_unlock = data; return;
        // case REG_RTC_FLAG: syscon->rtc_flag = data; return;
        case REG_WATCHDOG_FLAG2: syscon->watchdog_flag2 = data; return;
        case REG_BATTERY_LEVEL: syscon->battery_level = data; return;
        case REG_SRAM_WRITE_FLAG: syscon->sram_write_flag = data; return;
        case REG_SECURITY_UNLOCK_SET1: syscon->security_unlock_set1 = data; return;
        case REG_SECURITY_UNLOCK_SET2: syscon->security_unlock_set2 = data; return;
        default: return; // iris_debug(syscon, "Syscon: Unknown register write {:08x} {:08x}", addr, data); return;
    }
}

void destroy(Syscon* syscon) {
    delete syscon;
}

}
