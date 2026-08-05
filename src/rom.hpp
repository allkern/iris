#pragma once

#include <cstdint>
#include <cstddef>

namespace iris::rom {

enum class Type {
    ROM0,
    ROM1,
};

struct Info {
    char md5[33];
    const char* version;
    const char* region;
    const char* model;
    int system;
};

Info search(Type type, uint8_t* rom, size_t size);
bool is_valid(Type type, uint8_t* rom, size_t size);

}