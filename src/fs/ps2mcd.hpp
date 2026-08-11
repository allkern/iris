#pragma once

#include "fs/fs.hpp"

namespace iris::fs::ps2mcd {

Fs* open(logger::Logger* logger, blk::Device* dev, bool take_ownership);

}
