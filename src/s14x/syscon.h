// 10C00000 - 10C07FFF: S14X SRAM (32 KB)

#ifndef S14X_SYSCON_H
#define S14X_SYSCON_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdio.h>

#define S14X_SYSCON_REG_LED 1
#define S14X_SYSCON_REG_SECURITY_UNLOCK 2
#define S14X_SYSCON_REG_RTC_FLAG 4
#define S14X_SYSCON_REG_WATCHDOG_FLAG2 5
#define S14X_SYSCON_REG_BATTERY_LEVEL 6
#define S14X_SYSCON_REG_SRAM_WRITE_FLAG 7
#define S14X_SYSCON_REG_SECURITY_UNLOCK_SET1 12
#define S14X_SYSCON_REG_SECURITY_UNLOCK_SET2 13

#define S14X_RTC_STATE_READ_YEAR 0
#define S14X_RTC_STATE_READ_MONTH 1
#define S14X_RTC_STATE_READ_DAY 2
#define S14X_RTC_STATE_READ_DOW 3
#define S14X_RTC_STATE_READ_HOURS 4
#define S14X_RTC_STATE_READ_MINUTES 5
#define S14X_RTC_STATE_READ_SECONDS 6

struct s14x_syscon {
	uint8_t led;
	uint8_t security_unlock;
	uint8_t rtc_flag;
	uint8_t battery_level;
	uint8_t watchdog_flag2;
	uint8_t security_unlock_set1;
	uint8_t security_unlock_set2;
	int sram_write_flag;

	int rtc_state;
	int rtc_bit;
};

struct s14x_syscon* s14x_syscon_create(void);
void s14x_syscon_init(struct s14x_syscon* syscon);
uint64_t s14x_syscon_read(struct s14x_syscon* syscon, uint32_t addr);
void s14x_syscon_write(struct s14x_syscon* syscon, uint32_t addr, uint64_t data);
void s14x_syscon_destroy(struct s14x_syscon* syscon);

#ifdef __cplusplus
}
#endif

#endif