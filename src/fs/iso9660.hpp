#pragma once

#include "fs/fs.hpp"

namespace iris::fs::iso9660 {

Fs* open(logger::Logger* logger, blk::Device* dev, bool take_ownership);

}
