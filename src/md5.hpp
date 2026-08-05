#pragma once

#include <cstddef>
#include <cstdint>

namespace iris::md5 {

struct Md5 {
    uint64_t size;
    uint32_t state[4];
    uint8_t block[64];
    uint8_t digest[16];
};

void init(Md5* md5);
void update(Md5* md5, const uint8_t* data, size_t len);
void finalize(Md5* md5);

}
