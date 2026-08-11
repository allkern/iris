#pragma once

#include "fs/fs.hpp"
#include "fs/part.hpp"

namespace iris::fs::pfs {

Fs* open(logger::Logger* logger, blk::Device* dev, const std::vector <part::Extent>& extents, bool take_ownership);

}
