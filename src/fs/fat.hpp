#pragma once

#include "fs/fs.hpp"

namespace iris::fs::fat {

Fs* open(logger::Logger* logger, blk::Device* dev, bool take_ownership);

}
