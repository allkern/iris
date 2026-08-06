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

bool valid_ipv4(const std::string& s);
bool start(speed::smap::Smap* smap, const Config& cfg, LogSource* log);
void stop();
void restart(speed::smap::Smap* smap, const Config& cfg, LogSource* log);
bool running();
void pump(speed::smap::Smap* smap);

}
