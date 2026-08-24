#ifdef _WIN32
#define FD_SETSIZE 1024
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/select.h>
#include <sys/time.h>
#endif

#include <libslirp.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <deque>
#include <mutex>
#include <thread>
#include <vector>

#include "slirp.hpp"

#include "shared/speed/smap.hpp"

namespace iris::slirp {

struct slirp_timer {
    SlirpTimerId id;
    void* cb_opaque;
    int64_t expire_ms;
    bool active;
};

struct poll_entry {
    slirp_os_socket fd;
    int events;
};

struct state {
    Slirp* slirp = nullptr;
    speed::smap::Smap* smap = nullptr;

    SlirpCb cb;

    std::thread thread;
    std::atomic <bool> running{false};

    std::mutex slirp_mtx;
    std::mutex rxq_mtx;
    std::deque <std::vector<uint8_t>> rxq;

    std::vector <slirp_timer*> timers;

    LogSource* log = nullptr;

    std::vector <poll_entry> poll_entries;
    fd_set rfds, wfds, efds;
};

state* g = nullptr;

int64_t now_ns() {
    auto t = std::chrono::steady_clock::now().time_since_epoch();

    return std::chrono::duration_cast<std::chrono::nanoseconds>(t).count();
}

static bool parse_ipv4(const std::string& s, uint32_t* out) {
    unsigned int a, b, c, d;
    char extra;

    if (sscanf(s.c_str(), "%u.%u.%u.%u%c", &a, &b, &c, &d, &extra) != 4)
        return false;

    if (a > 255 || b > 255 || c > 255 || d > 255)
        return false;

    *out = (a << 24) | (b << 16) | (c << 8) | d;

    return true;
}

bool valid_ipv4(const std::string& s) {
    uint32_t tmp;

    return parse_ipv4(s, &tmp);
}

slirp_ssize_t cb_send_packet(const void* buf, size_t len, void* opaque) {
    (void)opaque;

    const uint8_t* p = (const uint8_t*)buf;

    // if (len >= 14)
    //     printf("slirp: -> len=%zu ethertype=%02x%02x\n", len, p[12], p[13]);

    std::lock_guard <std::mutex> lk(g->rxq_mtx);

    g->rxq.emplace_back(p, p + len);

    return (slirp_ssize_t)len;
}

void cb_guest_error(const char* msg, void* opaque) {
    (void)opaque;

    iris_error(g->log, "guest error: {}", msg);
}

int64_t cb_clock_get_ns(void* opaque) {
    (void)opaque;

    return now_ns();
}

void* cb_timer_new_opaque(SlirpTimerId id, void* cb_opaque, void* opaque) {
    (void)opaque;

    slirp_timer* t = new slirp_timer{ id, cb_opaque, 0, false };

    g->timers.push_back(t);

    return t;
}

void cb_timer_free(void* timer, void* opaque) {
    (void)opaque;

    slirp_timer* t = (slirp_timer*)timer;

    for (auto it = g->timers.begin(); it != g->timers.end(); ++it) {
        if (*it == t) {
            g->timers.erase(it);
            break;
        }
    }

    delete t;
}

void cb_timer_mod(void* timer, int64_t expire_time, void* opaque) {
    (void)opaque;

    slirp_timer* t = (slirp_timer*)timer;

    t->expire_ms = expire_time;
    t->active = true;
}

void cb_register_poll_fd(int fd, void* opaque) { (void)fd; (void)opaque; }
void cb_unregister_poll_fd(int fd, void* opaque) { (void)fd; (void)opaque; }
void cb_register_poll_socket(slirp_os_socket fd, void* opaque) { (void)fd; (void)opaque; }
void cb_unregister_poll_socket(slirp_os_socket fd, void* opaque) { (void)fd; (void)opaque; }
void cb_notify(void* opaque) { (void)opaque; }

int cb_add_poll(slirp_os_socket fd, int events, void* opaque) {
    (void)opaque;

    g->poll_entries.push_back({ fd, events });

    return (int)g->poll_entries.size() - 1;
}

int cb_get_revents(int idx, void* opaque) {
    (void)opaque;

    if (idx < 0 || idx >= (int)g->poll_entries.size())
        return 0;

    slirp_os_socket fd = g->poll_entries[idx].fd;

    int revents = 0;

    if (FD_ISSET(fd, &g->rfds)) revents |= SLIRP_POLL_IN;
    if (FD_ISSET(fd, &g->wfds)) revents |= SLIRP_POLL_OUT;
    if (FD_ISSET(fd, &g->efds)) revents |= SLIRP_POLL_ERR;

    return revents;
}

void fire_timers() {
    int64_t now_ms = now_ns() / 1000000;

    for (slirp_timer* t : g->timers) {
        if (t->active && t->expire_ms <= now_ms) {
            t->active = false;
            slirp_handle_timer(g->slirp, t->id, t->cb_opaque);
        }
    }
}

void poll_loop() {
    while (g->running.load()) {
        uint32_t timeout = 50;

        {
            std::lock_guard <std::mutex> lk(g->slirp_mtx);

            g->poll_entries.clear();
            slirp_pollfds_fill_socket(g->slirp, &timeout, cb_add_poll, g);
        }

        if (timeout > 50)
            timeout = 50;

        FD_ZERO(&g->rfds);
        FD_ZERO(&g->wfds);
        FD_ZERO(&g->efds);

        slirp_os_socket maxfd = 0;
        int nfds = 0;

        for (const poll_entry& e : g->poll_entries) {
            if (nfds >= FD_SETSIZE)
                break;

            if (e.events & (SLIRP_POLL_IN | SLIRP_POLL_PRI))
                FD_SET(e.fd, &g->rfds);
            if (e.events & SLIRP_POLL_OUT)
                FD_SET(e.fd, &g->wfds);

            FD_SET(e.fd, &g->efds);

            if (e.fd > maxfd)
                maxfd = e.fd;

            nfds++;
        }

        int ret = 0;

        if (nfds == 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(timeout));
        } else {
            struct timeval tv;
            tv.tv_sec = timeout / 1000;
            tv.tv_usec = (timeout % 1000) * 1000;

            ret = select((int)maxfd + 1, &g->rfds, &g->wfds, &g->efds, &tv);
        }

        {
            std::lock_guard <std::mutex> lk(g->slirp_mtx);

            slirp_pollfds_poll(g->slirp, ret < 0, cb_get_revents, g);
            fire_timers();
        }
    }
}

// Note: Ignore e-Amusement traffic. The servers are dead now, and
//       TD3 will fail to boot if it tries to connect but doesn't get
//       a response back
inline constexpr auto EAMUSE_UPDATE_PORT = 42197;

static const char* eamuse_hostname = "eamuse";

static bool dns_query_matches_eamuse(const uint8_t* dns, int len) {
    if (len < 13)
        return false;

    int offset = 12;

    while (offset < len) {
        int label = dns[offset];

        if (label == 0)
            return false;

        if (label > 63 || offset + 1 + label > len)
            return false;

        if (label == 6 && memcmp(dns + offset + 1, eamuse_hostname, 6) == 0)
            return true;

        offset += 1 + label;
    }

    return false;
}

static bool is_eamuse_traffic(const uint8_t* buf, int len) {
    if (len < 34)
        return false;

    if (buf[12] != 0x08 || buf[13] != 0x00)
        return false;

    const uint8_t* ip = buf + 14;

    if ((ip[0] >> 4) != 4)
        return false;

    int header_size = (ip[0] & 0x0f) * 4;

    if (header_size < 20 || 14 + header_size + 4 > len)
        return false;

    const uint8_t* payload = ip + header_size;

    int payload_size = len - (14 + header_size);
    int protocol = ip[9];
    int port = (payload[2] << 8) | payload[3];

    // TCP for the update session, UDP for the service discovery broadcast
    if ((protocol == 6 || protocol == 17) && port == EAMUSE_UPDATE_PORT)
        return true;

    if (protocol == 17 && port == 53 && payload_size > 8)
        return dns_query_matches_eamuse(payload + 8, payload_size - 8);

    return false;
}

void smap_tx(void* udata, const uint8_t* buf, int len) {
    (void)udata;

    if (is_eamuse_traffic(buf, len))
        return;

    std::lock_guard <std::mutex> lk(g->slirp_mtx);

    slirp_input(g->slirp, buf, len);
}

bool start(speed::smap::Smap* smap, const Config& cfg, LogSource* log) {
    if (getenv("IRIS_NO_NET")) {
        iris_info(log, "Disabled via IRIS_NO_NET");

        return false;
    }

    if (!cfg.enabled) {
        iris_info(log, "Networking disabled");

        return false;
    }

    if (g)
        return true;

#ifdef _WIN32
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
#endif

    g = new state();

    g->log = log;
    g->smap = smap;

    uint32_t network, netmask, gateway, dhcp, nameserver;

    if (!parse_ipv4(cfg.network,    &network))    network    = 0x0a000200; // 10.0.2.0
    if (!parse_ipv4(cfg.netmask,    &netmask))    netmask    = 0xffffff00; // 255.255.255.0
    if (!parse_ipv4(cfg.gateway,    &gateway))    gateway    = 0x0a000202; // 10.0.2.2
    if (!parse_ipv4(cfg.dhcp_start, &dhcp))       dhcp       = 0x0a00020f; // 10.0.2.15
    if (!parse_ipv4(cfg.nameserver, &nameserver)) nameserver = 0x0a000203; // 10.0.2.3

    SlirpConfig scfg;

    memset(&scfg, 0, sizeof(scfg));

    scfg.version = 4;
    scfg.in_enabled = true;
    scfg.vnetwork.s_addr = htonl(network);
    scfg.vnetmask.s_addr = htonl(netmask);
    scfg.vhost.s_addr = htonl(gateway);
    scfg.vdhcp_start.s_addr = htonl(dhcp);
    scfg.vnameserver.s_addr = htonl(nameserver);

    memset(&g->cb, 0, sizeof(g->cb));

    g->cb.send_packet = cb_send_packet;
    g->cb.guest_error = cb_guest_error;
    g->cb.clock_get_ns = cb_clock_get_ns;
    g->cb.timer_free = cb_timer_free;
    g->cb.timer_mod = cb_timer_mod;
    g->cb.register_poll_fd = cb_register_poll_fd;
    g->cb.unregister_poll_fd = cb_unregister_poll_fd;
    g->cb.notify = cb_notify;
    g->cb.timer_new_opaque = cb_timer_new_opaque;
    g->cb.register_poll_socket = cb_register_poll_socket;
    g->cb.unregister_poll_socket = cb_unregister_poll_socket;

    g->slirp = slirp_new(&scfg, &g->cb, g);

    if (!g->slirp) {
        delete g;

        g = nullptr;

        return false;
    }

    speed::smap::set_backend(smap, smap_tx, g);

    g->running.store(true);
    g->thread = std::thread(poll_loop);

    return true;
}

void stop() {
    if (!g)
        return;

    g->running.store(false);

    if (g->thread.joinable())
        g->thread.join();

    if (g->smap)
        speed::smap::set_backend(g->smap, nullptr, nullptr);

    {
        std::lock_guard <std::mutex> lk(g->slirp_mtx);

        if (g->slirp)
            slirp_cleanup(g->slirp);
    }

    for (slirp_timer* t : g->timers)
        delete t;

    delete g;
    g = nullptr;

#ifdef _WIN32
    WSACleanup();
#endif
}

void restart(speed::smap::Smap* smap, const Config& cfg, LogSource* log) {
    stop();
    start(smap, cfg, log);
}

bool running() {
    return g != nullptr;
}

void pump(speed::smap::Smap* smap) {
    if (!g)
        return;

    while (true) {
        std::vector <uint8_t> frame;

        {
            std::lock_guard <std::mutex> lk(g->rxq_mtx);

            if (g->rxq.empty())
                break;

            frame = std::move(g->rxq.front());
            g->rxq.pop_front();
        }

        if (!speed::smap::receive(smap, frame.data(), (int)frame.size())) {
            std::lock_guard <std::mutex> lk(g->rxq_mtx);

            g->rxq.push_front(std::move(frame));

            break;
        }
    }
}

}
