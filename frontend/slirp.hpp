#pragma once

#include <string>

struct ps2_smap;

namespace iris::slirp {

struct config {
    bool enabled = true;
    std::string network    = "10.0.2.0";
    std::string netmask    = "255.255.255.0";
    std::string gateway    = "10.0.2.2";
    std::string dhcp_start = "10.0.2.15";
    std::string nameserver = "10.0.2.3";
};

bool valid_ipv4(const std::string& s);

bool start(struct ps2_smap* smap, const config& cfg);
void stop();
void restart(struct ps2_smap* smap, const config& cfg);
bool running();
void pump(struct ps2_smap* smap);

}
