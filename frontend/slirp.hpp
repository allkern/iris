#pragma once

#include <string>

#include "log.hpp"

namespace iris::speed::smap { struct Smap; }

namespace iris::slirp {

struct Config {
    bool enabled = true;
    std::string network    = "10.0.2.0";
    std::string netmask    = "255.255.255.0";
    std::string gateway    = "10.0.2.2";
    std::string dhcp_start = "10.0.2.15";
    std::string nameserver = "10.0.2.3";
};

inline constexpr auto FIELD_NETWORK = 1 << 0;
inline constexpr auto FIELD_NETMASK = 1 << 1;
inline constexpr auto FIELD_GATEWAY = 1 << 2;
inline constexpr auto FIELD_DHCP_START = 1 << 3;
inline constexpr auto FIELD_NAMESERVER = 1 << 4;

struct Validation {
    int fields = 0;
    const char* reason = nullptr;
};

Validation validate(const Config& cfg);
bool start(speed::smap::Smap* smap, const Config& cfg, LogSource* log);
void stop();
void restart(speed::smap::Smap* smap, const Config& cfg, LogSource* log);
bool running();
void pump(speed::smap::Smap* smap);

}
