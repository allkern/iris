#pragma once

#include <unordered_map>
#include <cstdint>
#include <string>
#include <vector>
#include <chrono>
#include <array>
#include <deque>

#include <curl/curl.h>

namespace iris::net {

struct download_result {
    int status;
    std::string body;
};

bool init();
void cleanup();
download_result download(std::string url);

}