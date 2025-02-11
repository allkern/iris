#ifndef U128_H
#define U128_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>

typedef union {
    // unsigned __int128 u128;
    uint64_t u64[2];
    uint32_t u32[4];
    uint16_t u16[8];
    uint8_t u8[16];
    uint64_t ul64;
    uint32_t ul32;
    uint16_t ul16;
    uint8_t ul8;
    int64_t sl64;
    int32_t sl32;
    int16_t sl16;
    int8_t sl8;
} uint128_t;

#ifdef __cplusplus
}
#endif

#endif