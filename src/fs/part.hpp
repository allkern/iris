#pragma once

#include <string>
#include <vector>

#include "fs/blk.hpp"

namespace iris::fs::part {

struct Extent {
    uint64_t offset = 0;
    uint64_t size = 0;
};

struct Partition {
    uint64_t offset = 0;
    uint64_t size = 0;
    uint16_t type = 0;

    char name[72] = {};
    char type_name[24] = {};

    std::vector <Extent> extents;
};

// MBR and GPT only. Returns the number of usable partitions found
int scan(logger::Logger* logger, blk::Device* dev, std::vector <Partition>* out);

void set_mbr_type_name(Partition* partition);

}
