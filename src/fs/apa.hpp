#pragma once

#include "fs/part.hpp"

namespace iris::fs::apa {

int scan(logger::Logger* logger, blk::Device* dev, std::vector <part::Partition>* out);

const char* type_name(uint16_t type);

}
