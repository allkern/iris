#pragma once

#include <unordered_map>
#include <cstdint>
#include <string>
#include <vector>
#include <chrono>
#include <array>
#include <deque>

#include <curl/curl.h>

#include "log.hpp"

namespace iris::net {

struct DownloadResult {
    int status;
    std::string body;
};

bool init(LogSource* log);
void cleanup();
DownloadResult download(std::string url);

}