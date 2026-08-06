#pragma once

#include <cstddef>

#include "logger.hpp"

namespace iris {

struct LogSource {
    logger::Logger* logger = nullptr;
    size_t logger_id = 0;
};

}
