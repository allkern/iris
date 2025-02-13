#ifndef VU_H
#define VU_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

struct vu_reg {
    union {
        uint64_t u64[2];
        uint32_t u32[4];
        float f[4];

        // Named fields
        struct {
            float x;
            float y;
            float z;
            float w;
        };
    };
};

struct vu_state {
    struct vu_reg r[32];
    uint16_t i[16];

    struct vu_state* vu1;
};

#ifdef __cplusplus
}
#endif

#endif