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

#include <new>
#include <cstring>
#include <algorithm>
#include <filesystem>

#include "../speed.hpp"
#include "dvrp.hpp"

namespace iris::speed::dvrp {

namespace fs = std::filesystem;

enum : uint16_t {
    DVRF_CHSTAT = 0x1102,
    DVRF_CLOSE = 0x1103,
    DVRF_DCLOSE = 0x1104,
    DVRF_DEVCTL = 0x1105,
    DVRF_DOPEN = 0x1106,
    DVRF_DREAD = 0x1107,
    DVRF_FORMAT = 0x1108,
    DVRF_GETSTAT = 0x1109,
    DVRF_LSEEK = 0x110c,
    DVRF_LSEEK64 = 0x110d,
    DVRF_MKDIR = 0x110e,
    DVRF_MOUNT = 0x110f,
    DVRF_OPEN = 0x1110,
    DVRF_READ = 0x1111,
    DVRF_READLINK = 0x1112,
    DVRF_REMOVE = 0x1113,
    DVRF_RENAME = 0x1114,
    DVRF_RMDIR = 0x1115,
    DVRF_SYNC = 0x1117,
    DVRF_UMOUNT = 0x1118,
    DVRF_WRITE = 0x1119,
    DVR_REC_START = 0x2101,
    DVR_REC_PAUSE = 0x2102,
    DVR_REC_STOP = 0x2103,
    DVR_GET_REC_TIME = 0x2106,
    DVR_SEND_TIMER_EVENT = 0x2110,
    DVRAV_TUN_SCAN_CH = 0x3105,
    DVRAV_GET_BS_GAIN = 0x3106,
    DVRAV_SET_PRESET_INFO = 0x3107,
    DVRAV_GET_PRESET_INFO = 0x310c,
    DVRAV_SELECT_POSITION = 0x310e,
    DVRAV_POSITION_UP = 0x310f,
    DVRAV_POSITION_DOWN = 0x3110,
    DVRAV_GET_POSITION = 0x3111,
    DVR_DV_DUBB_REC_STOP = 0x4104,
    DVR_GET_DVCAM_NAME = 0x4108,
    DVR_GET_SIRCS = 0x5107,
    DVR_SAVE_PRESET_INFO = 0x510a
};

enum : int32_t {
    ERR_NOENT = -2,
    ERR_IO = -5,
    ERR_BADF = -9,
    ERR_NOTDIR = -20
};

static inline uint32_t be32(const uint8_t* p) {
    return (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3];
}

static inline void put_be32(uint8_t* p, uint32_t v) {
    p[0] = v >> 24;
    p[1] = v >> 16;
    p[2] = v >> 8;
    p[3] = v;
}

static inline void put_le32(uint8_t* p, uint32_t v) {
    p[0] = v;
    p[1] = v >> 8;
    p[2] = v >> 16;
    p[3] = v >> 24;
}

static inline uint32_t param32(Dvrp* dvrp, int index) {
    return (dvrp->params[index] << 16) | dvrp->params[index + 1];
}

static inline uint16_t base_of(uint16_t cmd) {
    return (cmd & 0xf0ff) | 0x0100;
}

static inline int phase_of(uint16_t cmd) {
    return (cmd >> 8) & 0xf;
}

static void update_irq(Dvrp* dvrp) {
    if (dvrp->intr_stat & dvrp->intr_mask) {
        send_irq(dvrp->speed, INTR_DVR);
    } else {
        dvrp->speed->intr_stat &= ~INTR_DVR;
    }
}

static void deliver_event(void* udata, int overshoot) {
    Dvrp* dvrp = (Dvrp*)udata;

    dvrp->delivery_pending = false;

    if (dvrp->responses.empty())
        return;

    Response r = dvrp->responses.front();

    dvrp->responses.pop_front();

    memset(dvrp->reply, 0, sizeof(dvrp->reply));
    memcpy(dvrp->reply, r.words, sizeof(r.words));

    dvrp->reply_count = r.count;
    dvrp->reply_index = 0;
    dvrp->intr_cause = r.cause;
    dvrp->intr_stat = r.stat;

    update_irq(dvrp);
}

static void schedule_delivery(Dvrp* dvrp) {
    if (dvrp->delivery_pending || dvrp->responses.empty() || (dvrp->intr_stat & dvrp->intr_mask))
        return;

    dvrp->delivery_pending = true;

    scheduler::Event event;

    event.callback = deliver_event;
    event.udata = dvrp;
    event.cycles = 2000;
    event.name = "dvrp response";

    scheduler::schedule(dvrp->speed->sched, event);
}

static void respond(Dvrp* dvrp, uint16_t cause, uint16_t stat, int count, const uint16_t* words = nullptr) {
    Response r;

    r.cause = cause;
    r.stat = stat;
    r.count = std::max(count, 1);

    if (words) {
        for (int i = 1; i < r.count && i < 8; i++) {
            r.words[i] = words[i];
        }
    }

    dvrp->responses.push_back(r);

    schedule_delivery(dvrp);
}

static void respond_value(Dvrp* dvrp, uint16_t cause, uint16_t stat, uint32_t value) {
    uint16_t words[3] = { 0, (uint16_t)(value >> 16), (uint16_t)(value & 0xffff) };

    respond(dvrp, cause, stat, 3, words);
}

static void ack(Dvrp* dvrp, uint16_t cmd) {
    respond(dvrp, cmd, INTR_CMD_ACK, 1);
}

static void complete(Dvrp* dvrp, uint16_t cmd, int32_t value) {
    respond_value(dvrp, base_of(cmd), INTR_CMD_COMP, (uint32_t)value);
}

static std::string host_path(Dvrp* dvrp, const char* guest) {
    std::string path(guest);

    for (char& c : path) {
        if (c == ':') {
            c = '_';
        } else if (c == '\\') {
            c = '/';
        }
    }

    fs::path result(dvrp->root);

    for (const auto& part : fs::path(path)) {
        std::string s = part.generic_string();

        if (s.empty() || s == "/" || s == "." || s == "..") {
            continue;
        }

        result /= part;
    }

    return result.string();
}

static std::string guest_string(const std::vector<uint8_t>& buf, size_t offset) {
    if (offset >= buf.size())
        return "";

    const char* begin = (const char*)buf.data() + offset;
    size_t len = strnlen(begin, buf.size() - offset);

    return std::string(begin, len);
}

static void put_time(uint8_t* p, fs::file_time_type ftime) {
    auto sys = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
        ftime - fs::file_time_type::clock::now() + std::chrono::system_clock::now()
    );

    std::time_t t = std::chrono::system_clock::to_time_t(sys);
    std::tm tm = *std::localtime(&t);

    uint16_t year = tm.tm_year + 1900;

    p[0] = 0;
    p[1] = tm.tm_sec;
    p[2] = tm.tm_min;
    p[3] = tm.tm_hour;
    p[4] = tm.tm_mday;
    p[5] = tm.tm_mon + 1;
    p[6] = year >> 8;
    p[7] = year & 0xff;
}

static int32_t fill_stat(const std::string& path, uint8_t* out, bool big_endian, bool root) {
    std::error_code ec;

    if (!fs::exists(path, ec))
        return ERR_NOENT;

    bool dir = fs::is_directory(path, ec);
    uint32_t mode = root ? 0x0100 : (dir ? 0x11ff : 0x21ff);
    uint32_t size = dir ? 0 : (uint32_t)fs::file_size(path, ec);

    memset(out, 0, 64);

    if (big_endian) {
        put_be32(&out[0], mode);
        put_be32(&out[8], size);
    } else {
        put_be32(&out[0], mode);
        put_le32(&out[8], size);
    }

    auto ftime = fs::last_write_time(path, ec);

    if (!ec) {
        put_time(&out[12], ftime);
        put_time(&out[20], ftime);
        put_time(&out[28], ftime);
    }

    return 0;
}

static DvrFile* get_file(Dvrp* dvrp, int fd) {
    auto it = dvrp->files.find(fd);

    return it == dvrp->files.end() ? nullptr : &it->second;
}

static int32_t do_open(Dvrp* dvrp) {
    uint32_t flags = be32(dvrp->dma_in.data());
    std::string guest = guest_string(dvrp->dma_in, 6);
    std::string path = host_path(dvrp, guest.c_str());

    std::error_code ec;
    bool exists = fs::exists(path, ec);
    bool write = (flags & 3) != 1;
    bool create = flags & 0x200;
    bool trunc = flags & 0x400;

    const char* mode = nullptr;

    if (exists && fs::is_directory(path, ec)) {
        return ERR_IO;
    }

    if (!exists && !create) {
        return ERR_NOENT;
    }

    if (!exists || trunc) {
        mode = "w+b";
    } else {
        mode = write ? "r+b" : "rb";
    }

    FILE* file = fopen(path.c_str(), mode);

    iris_debug(dvrp, "open {} ({:x}) -> {}", guest, flags, file ? "ok" : "fail");

    if (!file)
        return ERR_NOENT;

    int fd = dvrp->next_fd++;

    DvrFile& f = dvrp->files[fd];

    f.file = file;
    f.path = path;

    if (flags & 0x100) {
        fseek(file, 0, SEEK_END);
    }

    return fd;
}

static int32_t do_dopen(Dvrp* dvrp) {
    std::string guest = guest_string(dvrp->dma_in, 0);
    std::string path = host_path(dvrp, guest.c_str());

    std::error_code ec;

    if (!fs::exists(path, ec))
        return ERR_NOENT;

    if (!fs::is_directory(path, ec))
        return ERR_NOTDIR;

    int fd = dvrp->next_fd++;

    DvrFile& f = dvrp->files[fd];

    f.is_dir = true;
    f.path = path;
    f.root = guest.size() && guest.back() == ':' && guest.rfind("dvr_hdd", 0) == 0;
    f.entries.push_back(".");
    f.entries.push_back("..");

    for (const auto& entry : fs::directory_iterator(path, ec)) {
        f.entries.push_back(entry.path().filename().string());
    }

    iris_debug(dvrp, "dopen {} -> {} entries", guest, f.entries.size());

    return fd;
}

static int32_t do_close(Dvrp* dvrp, int fd) {
    DvrFile* f = get_file(dvrp, fd);

    if (!f)
        return ERR_BADF;

    if (f->file) {
        fclose(f->file);
    }

    dvrp->files.erase(fd);

    return 0;
}

static int32_t do_lseek(Dvrp* dvrp, int fd, int64_t offset, int whence) {
    DvrFile* f = get_file(dvrp, fd);

    if (!f || !f->file)
        return ERR_BADF;

    int origin = whence == 1 ? SEEK_CUR : (whence == 2 ? SEEK_END : SEEK_SET);

    if (fseek(f->file, (long)offset, origin))
        return ERR_IO;

    return (int32_t)ftell(f->file);
}

static int32_t do_read(Dvrp* dvrp) {
    DvrFile* f = get_file(dvrp, dvrp->file_fd);

    dvrp->dma_out.assign(dvrp->io_size, 0);

    if (!f || !f->file)
        return ERR_BADF;

    size_t n = fread(dvrp->dma_out.data(), 1, dvrp->io_size, f->file);

    return n ? (int32_t)n : -1;
}

static int32_t do_write(Dvrp* dvrp) {
    DvrFile* f = get_file(dvrp, dvrp->file_fd);

    if (!f || !f->file)
        return ERR_BADF;

    size_t size = std::min<size_t>(dvrp->io_size, dvrp->dma_in.size());

    return (int32_t)fwrite(dvrp->dma_in.data(), 1, size, f->file);
}

static int32_t do_dread(Dvrp* dvrp) {
    DvrFile* f = get_file(dvrp, dvrp->file_fd);

    dvrp->dma_out.assign(0x100, 0);

    if (!f || !f->is_dir)
        return ERR_BADF;

    if (f->index >= f->entries.size())
        return 0;

    const std::string& name = f->entries[f->index++];
    std::string path = (fs::path(f->path) / name).string();

    fill_stat(path, dvrp->dma_out.data(), false, f->root && name != "." && name != "..");

    size_t len = std::min<size_t>(name.size(), 0x100 - 64 - 1);

    memcpy(&dvrp->dma_out[64], name.data(), len);

    return (int32_t)len;
}

static int32_t do_getstat(Dvrp* dvrp) {
    std::string guest = guest_string(dvrp->dma_in, 0);
    std::string path = host_path(dvrp, guest.c_str());

    dvrp->dma_out.assign(0x100, 0);

    int32_t r = fill_stat(path, dvrp->dma_out.data(), true, false);

    iris_debug(dvrp, "getstat {} -> {}", guest, r);

    return r;
}

static int32_t do_mkdir(Dvrp* dvrp) {
    std::string guest = guest_string(dvrp->dma_in, 4);
    std::error_code ec;

    fs::create_directories(host_path(dvrp, guest.c_str()), ec);

    iris_debug(dvrp, "mkdir {}", guest);

    return 0;
}

static int32_t do_remove(Dvrp* dvrp, bool dir) {
    std::string guest = guest_string(dvrp->dma_in, 0);
    std::string path = host_path(dvrp, guest.c_str());
    std::error_code ec;

    if (!fs::exists(path, ec))
        return ERR_NOENT;

    if (fs::is_directory(path, ec) != dir)
        return dir ? ERR_NOTDIR : ERR_IO;

    return fs::remove(path, ec) ? 0 : ERR_IO;
}

static int32_t do_devctl(Dvrp* dvrp) {
    uint32_t ctlcmd = dvrp->dma_in.size() >= 8 ? be32(&dvrp->dma_in[4]) : 0;

    dvrp->dma_out.assign(0x100, 0);

    if (ctlcmd == 0x5001)
        return 0x80000;

    if (ctlcmd == 0x5002)
        return 0x7d00;

    return 0;
}

static int32_t do_dvcam_name(Dvrp* dvrp) {
    const char* name = "Sony Handycam";
    size_t len = strlen(name);

    dvrp->dma_out.assign(0x100, 0);
    dvrp->dma_out[0] = len >> 8;
    dvrp->dma_out[1] = len & 0xff;

    memcpy(&dvrp->dma_out[2], name, len);

    return 0;
}

static void handle_sircs(Dvrp* dvrp) {
    uint32_t code = dev::rm::next_code(dvrp->remote_key.load(), dvrp->remote_phase);

    dvrp->remote_reads++;

    uint16_t words[6] = {
        0,
        (uint16_t)(code ? 0x14 : 0x00),
        (uint16_t)(code & 0xff),
        (uint16_t)((code >> 8) & 0xff),
        (uint16_t)((code >> 16) & 0xff),
        0
    };

    respond(dvrp, DVR_GET_SIRCS, INTR_CMD_ACK, 6, words);
}

static void handle_phase1(Dvrp* dvrp, uint16_t cmd) {
    switch (cmd) {
        case DVR_GET_SIRCS: {
            handle_sircs(dvrp);
        } return;

        case DVRF_CLOSE:
        case DVRF_DCLOSE: {
            int32_t r = do_close(dvrp, param32(dvrp, 0));

            ack(dvrp, cmd);
            complete(dvrp, cmd, r);
        } return;

        case DVRF_LSEEK: {
            int32_t r = do_lseek(dvrp, param32(dvrp, 0), (int32_t)param32(dvrp, 2), param32(dvrp, 4));

            ack(dvrp, cmd);
            complete(dvrp, cmd, r);
        } return;

        case DVRF_LSEEK64: {
            int64_t offset = ((int64_t)param32(dvrp, 2) << 32) | param32(dvrp, 4);
            int64_t r = do_lseek(dvrp, param32(dvrp, 0), offset, param32(dvrp, 6));
            uint16_t words[5] = { 0, (uint16_t)(r >> 48), (uint16_t)(r >> 32), (uint16_t)(r >> 16), (uint16_t)r };

            ack(dvrp, cmd);
            respond(dvrp, base_of(cmd), INTR_CMD_COMP, 5, words);
        } return;

        case DVRF_READ:
        case DVRF_WRITE: {
            dvrp->file_fd = param32(dvrp, 0);
            dvrp->io_size = param32(dvrp, 2);

            ack(dvrp, cmd);
        } return;

        case DVRF_DREAD: {
            dvrp->file_fd = param32(dvrp, 0);

            ack(dvrp, cmd);
        } return;

        case DVR_REC_START: {
            dvrp->rec_active = true;
            dvrp->rec_paused = false;

            time(&dvrp->rec_start);

            ack(dvrp, cmd);
            complete(dvrp, cmd, 0);
        } return;

        case DVR_REC_STOP: {
            dvrp->rec_active = false;
            dvrp->rec_paused = false;

            ack(dvrp, cmd);
            complete(dvrp, cmd, 0);
        } return;

        case DVR_REC_PAUSE: {
            dvrp->rec_paused = !dvrp->params[0];

            ack(dvrp, cmd);
        } return;

        case DVR_GET_REC_TIME: {
            std::time_t now;

            time(&now);

            respond_value(dvrp, cmd, INTR_CMD_ACK, (uint32_t)difftime(now, dvrp->rec_start));
        } return;

        case DVR_DV_DUBB_REC_STOP:
        case DVR_SAVE_PRESET_INFO: {
            ack(dvrp, cmd);
            complete(dvrp, cmd, 0);
        } return;

        case DVRAV_TUN_SCAN_CH: {
            uint16_t words[6] = { 0, 0x1234, 0x5678, 0, 0, 0 };

            ack(dvrp, cmd);
            respond(dvrp, base_of(cmd), INTR_CMD_COMP, 6, words);
        } return;

        case DVRAV_SELECT_POSITION: {
            dvrp->tv_channel = dvrp->params[0];

            ack(dvrp, cmd);
        } return;

        case DVRAV_POSITION_UP: {
            dvrp->tv_channel++;

            ack(dvrp, cmd);
        } return;

        case DVRAV_POSITION_DOWN: {
            dvrp->tv_channel--;

            ack(dvrp, cmd);
        } return;

        case DVRAV_GET_POSITION: {
            uint16_t words[2] = { 0, dvrp->tv_channel };

            respond(dvrp, cmd, INTR_CMD_ACK, 2, words);
        } return;

        case DVRAV_GET_BS_GAIN: {
            uint16_t words[2] = { 0, (uint16_t)(100 - dvrp->tv_channel / 10) };

            respond(dvrp, cmd, INTR_CMD_ACK, 2, words);
        } return;

        case DVRAV_SET_PRESET_INFO: {
            dvrp->presets[dvrp->params[0]] = dvrp->params[1];

            ack(dvrp, cmd);
        } return;

        case DVRAV_GET_PRESET_INFO: {
            uint16_t words[2] = { 0, dvrp->presets[dvrp->params[0]] };

            respond(dvrp, cmd, INTR_CMD_ACK, 2, words);
        } return;
    }

    ack(dvrp, cmd);
}

static void handle_phase3(Dvrp* dvrp, uint16_t cmd) {
    uint16_t base = base_of(cmd);

    switch (base) {
        case DVRF_READ: dvrp->ret = do_read(dvrp); break;
        case DVRF_DREAD: dvrp->ret = do_dread(dvrp); break;
        case DVRF_GETSTAT: dvrp->ret = do_getstat(dvrp); break;
        case DVRF_DEVCTL: dvrp->ret = do_devctl(dvrp); break;
        case DVR_GET_DVCAM_NAME: dvrp->ret = do_dvcam_name(dvrp); break;

        case DVRF_CHSTAT: {
            dvrp->dma_out.assign(0x100, 0);
            dvrp->ret = 0;
        } break;

        default: {
            iris_warning(dvrp, "Unhandled phase 3 command {:04x}", cmd);

            dvrp->dma_out.assign(0x100, 0);
            dvrp->ret = ERR_IO;
        } break;
    }

    dvrp->dma_cmd = cmd;

    respond_value(dvrp, cmd, INTR_DMA_ACK, (uint32_t)dvrp->dma_out.size());
}

static bool has_phase3(uint16_t base) {
    switch (base) {
        case DVRF_GETSTAT:
        case DVRF_CHSTAT:
        case DVRF_DEVCTL:
        case DVRF_READLINK:
            return true;
    }

    return false;
}

static void finish_send(Dvrp* dvrp) {
    uint16_t base = base_of(dvrp->dma_cmd);

    if (has_phase3(base))
        return;

    int32_t r = 0;

    switch (base) {
        case DVRF_OPEN: r = do_open(dvrp); break;
        case DVRF_DOPEN: r = do_dopen(dvrp); break;
        case DVRF_MKDIR: r = do_mkdir(dvrp); break;
        case DVRF_REMOVE: r = do_remove(dvrp, false); break;
        case DVRF_RMDIR: r = do_remove(dvrp, true); break;
        case DVRF_WRITE: r = do_write(dvrp); break;
        case DVRF_SYNC:
        case DVRF_UMOUNT:
        case DVRF_MOUNT:
        case DVRF_FORMAT:
        case DVR_SEND_TIMER_EVENT: r = 0; break;

        default: {
            iris_warning(dvrp, "Unhandled DMA send command {:04x}", dvrp->dma_cmd);

            r = ERR_IO;
        } break;
    }

    complete(dvrp, dvrp->dma_cmd, r);
}

static void handle_command(Dvrp* dvrp, uint16_t cmd) {
    dvrp->cmd = cmd;

    if (cmd != DVR_GET_SIRCS) {
        iris_debug(dvrp, "Command {:04x} ({} params)", cmd, dvrp->param_index);
    }

    switch (phase_of(cmd)) {
        case 1: {
            handle_phase1(dvrp, cmd);
        } break;

        case 2: {
            dvrp->dma_cmd = cmd;
            dvrp->dma_in.clear();

            respond(dvrp, cmd, INTR_DMA_ACK, 1);
        } break;

        case 3: {
            handle_phase3(dvrp, cmd);
        } break;

        default: {
            iris_warning(dvrp, "Unhandled command {:04x}", cmd);

            ack(dvrp, cmd);
        } break;
    }

    dvrp->param_index = 0;
}

static void ready_event(void* udata, int overshoot) {
    Dvrp* dvrp = (Dvrp*)udata;

    respond(dvrp, 0, INTR_READY, 1);
}

static void write_reset(Dvrp* dvrp, uint8_t data) {
    bool rising = !(dvrp->reset_reg & 1) && (data & 1);

    dvrp->reset_reg = data;

    if (!rising)
        return;

    scheduler::Event event;

    event.callback = ready_event;
    event.udata = dvrp;
    event.cycles = 100000;
    event.name = "dvrp ready";

    scheduler::schedule(dvrp->speed->sched, event);
}

Dvrp* create(logger::Logger* logger) {
    Dvrp* dvrp = new Dvrp();

    dvrp->logger = logger;
    dvrp->logger_id = logger::register_source(logger, "dvrp");

    return dvrp;
}

void init(Dvrp* dvrp, Speed* speed) {
    dvrp->speed = speed;
}

void destroy(Dvrp* dvrp) {
    for (auto& [fd, f] : dvrp->files) {
        if (f.file) {
            fclose(f.file);
        }
    }

    delete dvrp;
}

uint64_t read(Dvrp* dvrp, uint32_t addr) {
    switch (addr) {
        case 0x4004: return dvrp->dma_dir;
        case 0x4008: return dvrp->reset_reg;
        case 0x4100: return 0;
        case 0x4200: {
            dvrp->reply_index = 0;

            return dvrp->intr_stat;
        }
        case 0x4208: {
            dvrp->reply_index = 0;

            return dvrp->intr_mask;
        }
        case 0x4210: return dvrp->cmd;
        case 0x4218: return 0;
        case 0x4220: {
            dvrp->reply_index = 0;

            return dvrp->intr_cause;
        }
        case 0x4224: return dvrp->reply_index < 64 ? dvrp->reply[dvrp->reply_index++] : 0;
        case 0x4228: {
            dvrp->reply_index = 0;

            return dvrp->reply_count << 2;
        }
        case 0x4230: return 0x3e;
        case 0x4234: return 0x01 | (dvrp->rec_paused ? 0x02 : 0x00) | (dvrp->rec_active ? 0x04 : 0x00);
        case 0x4238: return 0;
        case 0x423c: return 0;
    }

    iris_debug(dvrp, "Unhandled read {:04x}", addr);

    return 0;
}

void write(Dvrp* dvrp, uint32_t addr, uint64_t data) {
    switch (addr) {
        case 0x4004: dvrp->dma_dir = data; return;
        case 0x4008: write_reset(dvrp, data); return;
        case 0x4100:
        case 0x4108:
        case 0x410c:
        case 0x4218:
        case 0x4230: return;
        case 0x4120:
        case 0x4122: {
            iris_debug(dvrp, "DMA PIO tail {:04x}", data);
        } return;
        case 0x4204: {
            dvrp->intr_stat &= ~data;

            update_irq(dvrp);
            schedule_delivery(dvrp);
        } return;
        case 0x4208: {
            dvrp->intr_mask = data;

            update_irq(dvrp);
            schedule_delivery(dvrp);
        } return;
        case 0x4210: handle_command(dvrp, data); return;
        case 0x4214: {
            if (dvrp->param_index < 0x10) {
                dvrp->params[dvrp->param_index++] = data;
            }
        } return;
    }

    iris_debug(dvrp, "Unhandled write {:04x} ({:04x})", addr, data);
}

void read_dma(Dvrp* dvrp, uint8_t* buf, uint32_t size) {
    memset(buf, 0, size);
    memcpy(buf, dvrp->dma_out.data(), std::min<size_t>(size, dvrp->dma_out.size()));

    respond(dvrp, dvrp->dma_cmd, INTR_DMA_END, 1);
    complete(dvrp, dvrp->dma_cmd, dvrp->ret);
}

void write_dma(Dvrp* dvrp, const uint8_t* buf, uint32_t size) {
    dvrp->dma_in.insert(dvrp->dma_in.end(), buf, buf + size);

    respond(dvrp, dvrp->dma_cmd, INTR_DMA_END, 1);

    finish_send(dvrp);
}

void set_root(Dvrp* dvrp, const char* path) {
    dvrp->root = path;

    std::error_code ec;

    fs::create_directories(fs::path(dvrp->root) / "dvr_hdd0_", ec);
}

void set_remote_key(Dvrp* dvrp, dev::rm::Key key) {
    dvrp->remote_key.store(key);
}

void release_remote_key(Dvrp* dvrp) {
    dvrp->remote_key.store(dev::rm::Key {});
}

}
