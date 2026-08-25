#pragma once

#include <cstdint>

#include "logger.hpp"

namespace iris::fw::device {

inline constexpr auto RESP_COMPLETE = 0;
inline constexpr auto RESP_ADDRESS_ERROR = -1;
inline constexpr auto RESP_TYPE_ERROR = -2;

struct Device;

struct Ops {
    int (*read)(Device* dev, uint64_t offset, uint8_t* buf, int len);
    int (*write)(Device* dev, uint64_t offset, const uint8_t* buf, int len);
    void (*reset)(Device* dev);
    void (*free)(Device* dev);
};

struct Device {
    int connected;

    uint16_t node_id;
    uint64_t guid;

    const Ops* ops;
    void* priv;

    logger::Logger* logger = nullptr;
};

int read(Device* dev, uint64_t offset, uint8_t* buf, int len);
int write(Device* dev, uint64_t offset, const uint8_t* buf, int len);
void reset(Device* dev);
void free(Device* dev);

}
