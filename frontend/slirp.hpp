/*
    This file is part of the Iris PlayStation 2 emulator project.
    Hosted at https://github.com/allkern/iris

    MIT License

    Copyright (c) 2026 Allkern/Lycoder (Lisandro Alarcón)

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in all
    copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
    SOFTWARE.
*/

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
