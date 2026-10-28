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

#include <algorithm>
#include <chrono>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <type_traits>
#include <vector>

#include "iris.hpp"
#include "ee/ee_def.hpp"
#include "ps2.hpp"

extern "C" {
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
}

namespace iris::script {

constexpr auto TIME_LIMIT = std::chrono::seconds(5);
constexpr int HOOK_INTERVAL = 1000000;
constexpr int DEFAULT_TAP_FRAMES = 4;

enum Wait {
    WAIT_NONE,
    WAIT_FRAMES,
    WAIT_UNTIL
};

struct Tap {
    int slot;
    uint16_t mask;
    int frames;
};

struct Target {
    uint8_t* ptr;
    uint32_t phys;
    uint32_t avail;
    bool spr;
};

struct Script {
    Instance* iris = nullptr;
    lua_State* L = nullptr;
    lua_State* body = nullptr;
    std::string name = "";
    std::string error = "";
    uint64_t frame = 0;

    int wait = WAIT_NONE;
    int64_t wait_frames = 0;
    int wait_fn = LUA_NOREF;
    bool done = false;
    bool failed = false;

    std::vector <int> on_frame;
    std::vector <Tap> taps;
    uint16_t pressed[2] = {};
    std::vector <std::string> screenshots;

    bool quit = false;
    bool quit_ok = true;

    std::chrono::steady_clock::time_point call_start;
};

static const char* const button_names[] = {
    "select", "l3", "r3", "start",
    "up", "right", "down", "left",
    "l2", "r2", "l1", "r1",
    "triangle", "circle", "cross", "square",
    nullptr
};

static const char* const axis_names[] = {
    "lx", "ly", "rx", "ry",
    nullptr
};

static const int axis_ids[] = {
    dev::ds::LEFT_H,
    dev::ds::LEFT_V,
    dev::ds::RIGHT_H,
    dev::ds::RIGHT_V
};

static Script* get(lua_State* L) {
    return *(Script**)lua_getextraspace(L);
}

static void fail(Script* s, const char* msg) {
    s->failed = true;
    s->error = msg ? msg : "(error object is not a string)";

    iris_error(&s->iris->log.script, "{}", s->error);

    push_info(s->iris, "Script error, see the log for details");

    if (s->iris->headless) {
        s->quit = true;
        s->quit_ok = false;
    }
}

static int traceback(lua_State* L) {
    const char* msg = lua_tostring(L, 1);

    luaL_traceback(L, L, msg ? msg : "(error object is not a string)", 1);

    return 1;
}

static bool call(Script* s, int nargs, int nresults) {
    int base = lua_gettop(s->L) - nargs;

    lua_pushcfunction(s->L, traceback);
    lua_insert(s->L, base);

    s->call_start = std::chrono::steady_clock::now();

    int status = lua_pcall(s->L, nargs, nresults, base);

    lua_remove(s->L, base);

    if (status != LUA_OK) {
        fail(s, lua_tostring(s->L, -1));

        lua_pop(s->L, 1);

        return false;
    }

    return true;
}

static void resume(Script* s, int nargs) {
    int nresults = 0;

    s->wait = WAIT_NONE;
    s->call_start = std::chrono::steady_clock::now();

    int status = lua_resume(s->body, s->L, nargs, &nresults);

    if (status == LUA_OK || status == LUA_YIELD) {
        lua_pop(s->body, nresults);

        s->done = status == LUA_OK;

        return;
    }

    const char* msg = lua_tostring(s->body, -1);

    luaL_traceback(s->L, s->body, msg ? msg : "(error object is not a string)", 0);

    fail(s, lua_tostring(s->L, -1));

    lua_pop(s->L, 1);
}

static void count_hook(lua_State* L, lua_Debug*) {
    Script* s = get(L);

    if (std::chrono::steady_clock::now() - s->call_start > TIME_LIMIT)
        luaL_error(L, "script ran for over %d seconds without returning or waiting", (int)TIME_LIMIT.count());
}

static void apply_pads(Script* s) {
    for (int slot = 0; slot < 2; slot++) {
        dev::ds::Ds* ds = s->iris->input.ds[slot];

        if (!ds) {
            continue;
        }

        uint16_t mask = s->pressed[slot];

        for (const Tap& tap : s->taps) {
            if (tap.slot == slot) {
                mask |= tap.mask;
            }
        }

        ds->held = mask;
    }
}

static void reset_pads(Script* s) {
    for (int slot = 0; slot < 2; slot++) {
        dev::ds::Ds* ds = s->iris->input.ds[slot];

        if (!ds) {
            continue;
        }

        ds->held = 0;

        for (int& axis : ds->axis_override) {
            axis = -1;
        }
    }
}

static void tick_taps(Script* s) {
    if (s->taps.empty())
        return;

    for (Tap& tap : s->taps) {
        tap.frames--;
    }

    std::erase_if(s->taps, [](const Tap& tap) { return tap.frames <= 0; });

    apply_pads(s);
}

static void close_state(Script* s) {
    if (!s->L)
        return;

    lua_close(s->L);

    s->L = nullptr;
    s->body = nullptr;
    s->wait = WAIT_NONE;
    s->wait_fn = LUA_NOREF;
    s->on_frame.clear();
    s->taps.clear();
    s->pressed[0] = 0;
    s->pressed[1] = 0;

    reset_pads(s);
}

static void address_error(lua_State* L, const char* what, uint32_t addr) {
    char buf[16];

    snprintf(buf, sizeof(buf), "0x%08x", addr);

    luaL_error(L, "address %s is %s", buf, what);
}

static Target check_target(lua_State* L, uint32_t size) {
    Script* s = get(L);
    ps2::Ps2* ps2 = s->iris->ps2;

    uint32_t addr = (uint32_t)luaL_checkinteger(L, 1);
    uint32_t seg = addr >> 28;

    if (seg == 0x7) {
        uint32_t offset = addr & 0x0fffffff;

        if ((uint64_t)offset + size > ps2->ee->spr->size) {
            address_error(L, "outside the scratchpad", addr);
        }

        return { ps2->ee->spr->buf + offset, offset, (uint32_t)(ps2->ee->spr->size - offset), true };
    }

    uint32_t phys;

    switch (seg) {
        case 0x0: case 0x2: case 0x3: phys = addr & 0x0fffffff; break;
        case 0x8: case 0xa: phys = addr & 0x1fffffff; break;
        default: phys = 0xffffffff; break;
    }

    if ((uint64_t)phys + size > ps2->ee_ram->size) {
        address_error(L, "not in EE RAM or the scratchpad", addr);
    }

    return { ps2->ee_ram->buf + phys, phys, (uint32_t)(ps2->ee_ram->size - phys), false };
}

static void invalidate(Script* s, const Target& target, uint32_t size) {
    ee::Ee* ee = s->iris->ps2->ee;

    if (target.spr) {
        ee::invalidate_spr(ee);

        return;
    }

    ee::invalidate_block(ee, target.phys);
    ee::invalidate_block(ee, target.phys + size - 1);
}

template <typename T> static int l_read(lua_State* L) {
    Target target = check_target(L, sizeof(T));

    T value;

    std::memcpy(&value, target.ptr, sizeof(T));

    if constexpr (std::is_floating_point_v <T>) {
        lua_pushnumber(L, value);
    } else {
        lua_pushinteger(L, (lua_Integer)value);
    }

    return 1;
}

template <typename T> static int l_write(lua_State* L) {
    Target target = check_target(L, sizeof(T));

    T value;

    if constexpr (std::is_floating_point_v <T>) {
        value = (T)luaL_checknumber(L, 2);
    } else {
        value = (T)luaL_checkinteger(L, 2);
    }

    std::memcpy(target.ptr, &value, sizeof(T));

    invalidate(get(L), target, sizeof(T));

    return 0;
}

static int l_read_string(lua_State* L) {
    Target target = check_target(L, 1);

    lua_Integer max = luaL_optinteger(L, 2, 256);

    luaL_argcheck(L, max >= 0, 2, "must not be negative");

    size_t limit = std::min <size_t>((size_t)max, target.avail);

    const char* str = (const char*)target.ptr;

    lua_pushlstring(L, str, strnlen(str, limit));

    return 1;
}

static int check_slot(lua_State* L) {
    lua_Integer port = luaL_checkinteger(L, 1);

    luaL_argcheck(L, port == 1 || port == 2, 1, "port must be 1 or 2");

    if (!get(L)->iris->input.ds[port - 1]) {
        luaL_error(L, "no controller on port %d", (int)port);
    }

    return (int)port - 1;
}

static uint16_t check_button(lua_State* L, int arg) {
    return 1 << luaL_checkoption(L, arg, nullptr, button_names);
}

static uint16_t check_buttons(lua_State* L, int first) {
    int top = lua_gettop(L);

    if (top < first) {
        luaL_error(L, "expected at least one button");
    }

    uint16_t mask = 0;

    for (int i = first; i <= top; i++) {
        mask |= check_button(L, i);
    }

    return mask;
}

static int l_pad_press(lua_State* L) {
    Script* s = get(L);

    int slot = check_slot(L);

    s->pressed[slot] |= check_buttons(L, 2);

    apply_pads(s);

    return 0;
}

static int l_pad_release(lua_State* L) {
    Script* s = get(L);

    int slot = check_slot(L);

    s->pressed[slot] &= ~check_buttons(L, 2);

    apply_pads(s);

    return 0;
}

static int l_pad_tap(lua_State* L) {
    Script* s = get(L);

    int slot = check_slot(L);
    uint16_t mask = check_button(L, 2);
    lua_Integer frames = luaL_optinteger(L, 3, DEFAULT_TAP_FRAMES);

    luaL_argcheck(L, frames >= 1, 3, "must be at least 1");

    s->taps.push_back({ slot, mask, (int)frames });

    apply_pads(s);

    return 0;
}

static int l_pad_axis(lua_State* L) {
    Script* s = get(L);

    int slot = check_slot(L);
    int axis = axis_ids[luaL_checkoption(L, 2, nullptr, axis_names)];
    int value = -1;

    if (!lua_isnoneornil(L, 3)) {
        lua_Integer v = luaL_checkinteger(L, 3);

        luaL_argcheck(L, v >= 0 && v <= 255, 3, "must be between 0 and 255");

        value = (int)v;
    }

    s->iris->input.ds[slot]->axis_override[axis] = value;

    return 0;
}

static int l_pad_clear(lua_State* L) {
    Script* s = get(L);

    int slot = check_slot(L);

    s->pressed[slot] = 0;

    std::erase_if(s->taps, [slot](const Tap& tap) { return tap.slot == slot; });

    for (int& axis : s->iris->input.ds[slot]->axis_override) {
        axis = -1;
    }

    apply_pads(s);

    return 0;
}

static int l_wait(lua_State* L) {
    Script* s = get(L);

    lua_Integer frames = luaL_optinteger(L, 1, 1);

    luaL_argcheck(L, frames >= 1, 1, "must be at least 1");

    if (L != s->body) {
        luaL_error(L, "iris.wait can only be called from the script body");
    }

    s->wait = WAIT_FRAMES;
    s->wait_frames = frames;

    return lua_yield(L, 0);
}

static int l_wait_until(lua_State* L) {
    Script* s = get(L);

    luaL_checktype(L, 1, LUA_TFUNCTION);

    lua_Integer timeout = luaL_optinteger(L, 2, 0);

    luaL_argcheck(L, timeout >= 0, 2, "must not be negative");

    if (L != s->body) {
        luaL_error(L, "iris.wait_until can only be called from the script body");
    }

    lua_pushvalue(L, 1);

    s->wait = WAIT_UNTIL;
    s->wait_frames = timeout;
    s->wait_fn = luaL_ref(L, LUA_REGISTRYINDEX);

    return lua_yield(L, 0);
}

static int l_frame(lua_State* L) {
    lua_pushinteger(L, (lua_Integer)get(L)->frame);

    return 1;
}

static int l_on_frame(lua_State* L) {
    luaL_checktype(L, 1, LUA_TFUNCTION);

    lua_pushvalue(L, 1);

    get(L)->on_frame.push_back(luaL_ref(L, LUA_REGISTRYINDEX));

    return 0;
}

static int l_screenshot(lua_State* L) {
    const char* name = luaL_optstring(L, 1, "");

    get(L)->screenshots.emplace_back(name);

    return 0;
}

static int l_log(lua_State* L) {
    int top = lua_gettop(L);

    for (int i = 1; i <= top; i++) {
        if (i > 1) {
            lua_pushliteral(L, "\t");
        }

        luaL_tolstring(L, i, nullptr);
    }

    lua_concat(L, top ? top * 2 - 1 : 0);

    iris_info(&get(L)->iris->log.script, "{}", lua_tostring(L, -1));

    return 0;
}

static int l_quit(lua_State* L) {
    Script* s = get(L);

    s->quit = true;
    s->quit_ok = lua_isnoneornil(L, 1) || lua_toboolean(L, 1);

    return 0;
}

static const luaL_Reg iris_funcs[] = {
    { "wait", l_wait },
    { "wait_until", l_wait_until },
    { "frame", l_frame },
    { "on_frame", l_on_frame },
    { "screenshot", l_screenshot },
    { "log", l_log },
    { "quit", l_quit },
    { nullptr, nullptr }
};

static const luaL_Reg mem_funcs[] = {
    { "read8", l_read <uint8_t> },
    { "read16", l_read <uint16_t> },
    { "read32", l_read <uint32_t> },
    { "read64", l_read <uint64_t> },
    { "readf", l_read <float> },
    { "write8", l_write <uint8_t> },
    { "write16", l_write <uint16_t> },
    { "write32", l_write <uint32_t> },
    { "write64", l_write <uint64_t> },
    { "writef", l_write <float> },
    { "read_string", l_read_string },
    { nullptr, nullptr }
};

static const luaL_Reg pad_funcs[] = {
    { "press", l_pad_press },
    { "release", l_pad_release },
    { "tap", l_pad_tap },
    { "axis", l_pad_axis },
    { "clear", l_pad_clear },
    { nullptr, nullptr }
};

static const luaL_Reg libs[] = {
    { LUA_GNAME, luaopen_base },
    { LUA_COLIBNAME, luaopen_coroutine },
    { LUA_TABLIBNAME, luaopen_table },
    { LUA_STRLIBNAME, luaopen_string },
    { LUA_MATHLIBNAME, luaopen_math },
    { LUA_UTF8LIBNAME, luaopen_utf8 },
    { nullptr, nullptr }
};

static void open_libs(lua_State* L) {
    for (const luaL_Reg* lib = libs; lib->func; lib++) {
        luaL_requiref(L, lib->name, lib->func, 1);
        lua_pop(L, 1);
    }

    lua_getglobal(L, LUA_MATHLIBNAME);
    lua_getfield(L, -1, "randomseed");
    lua_pushinteger(L, 0);
    lua_call(L, 1, 0);
    lua_pop(L, 1);

    luaL_newlib(L, iris_funcs);

    luaL_newlib(L, mem_funcs);
    lua_setfield(L, -2, "mem");

    luaL_newlib(L, pad_funcs);
    lua_setfield(L, -2, "pad");

    lua_setglobal(L, "iris");

    lua_pushcfunction(L, l_log);
    lua_setglobal(L, "print");
}

static void step_body(Script* s) {
    if (s->done || s->failed)
        return;

    if (s->wait == WAIT_FRAMES) {
        if (--s->wait_frames > 0)
            return;

        resume(s, 0);

        return;
    }

    if (s->wait == WAIT_UNTIL) {
        lua_rawgeti(s->L, LUA_REGISTRYINDEX, s->wait_fn);

        if (!call(s, 0, 1))
            return;

        bool met = lua_toboolean(s->L, -1);

        lua_pop(s->L, 1);

        bool timed_out = s->wait_frames > 0 && --s->wait_frames == 0;

        if (!met && !timed_out)
            return;

        luaL_unref(s->L, LUA_REGISTRYINDEX, s->wait_fn);

        s->wait_fn = LUA_NOREF;

        lua_pushboolean(s->body, met);

        resume(s, 1);

        return;
    }

    resume(s, 0);
}

static bool start(Instance* iris, const std::string& source, const std::string& name, const std::string& chunkname) {
    unload(iris);

    Script* s = new Script();

    s->iris = iris;
    s->name = name;
    s->L = luaL_newstate();

    iris->script = s;

    if (!s->L) {
        iris_error(&iris->log.script, "Couldn't create a Lua state");

        unload(iris);

        return false;
    }

    *(Script**)lua_getextraspace(s->L) = s;

    open_libs(s->L);

    lua_sethook(s->L, count_hook, LUA_MASKCOUNT, HOOK_INTERVAL);

    if (luaL_loadbuffer(s->L, source.data(), source.size(), chunkname.c_str()) != LUA_OK) {
        fail(s, lua_tostring(s->L, -1));
        close_state(s);

        return false;
    }

    s->body = lua_newthread(s->L);

    luaL_ref(s->L, LUA_REGISTRYINDEX);
    lua_xmove(s->L, s->body, 1);

    iris_info(&iris->log.script, "Loaded script \"{}\"", name);

    resume(s, 0);

    if (s->failed) {
        close_state(s);

        return false;
    }

    return true;
}

bool load(Instance* iris, const std::string& path) {
    std::ifstream file(path, std::ios::binary);

    if (!file) {
        iris_error(&iris->log.script, "Couldn't open script \"{}\"", path);

        return false;
    }

    std::string source((std::istreambuf_iterator <char>(file)), std::istreambuf_iterator <char>());

    return start(iris, source, path, "@" + path);
}

bool load_source(Instance* iris, const std::string& source, const std::string& name) {
    return start(iris, source, name, "=" + name);
}

void unload(Instance* iris) {
    Script* s = iris->script;

    if (!s) return;

    close_state(s);

    delete s;

    iris->script = nullptr;
}

bool is_loaded(Instance* iris) {
    return iris->script && iris->script->L;
}

State get_state(Instance* iris) {
    Script* s = iris->script;

    if (!s) {
        return STATE_IDLE;
    }

    if (s->failed) {
        return STATE_FAILED;
    }

    if (!s->L) {
        return STATE_IDLE;
    }

    return s->done && s->on_frame.empty() ? STATE_DONE : STATE_RUNNING;
}

std::string get_name(Instance* iris) {
    return iris->script ? iris->script->name : "";
}

std::string get_error(Instance* iris) {
    return iris->script ? iris->script->error : "";
}

uint64_t get_frame(Instance* iris) {
    return iris->script ? iris->script->frame : 0;
}

void frame(Instance* iris) {
    Script* s = iris->script;

    if (!s || !s->L)
        return;

    s->frame++;

    tick_taps(s);

    size_t count = s->on_frame.size();

    for (size_t i = 0; i < count && !s->failed; i++) {
        lua_rawgeti(s->L, LUA_REGISTRYINDEX, s->on_frame[i]);

        call(s, 0, 0);
    }

    step_body(s);

    if (s->failed) {
        close_state(s);
    }
}

SDL_AppResult flush(Instance* iris) {
    Script* s = iris->script;

    if (!s) {
        return SDL_APP_CONTINUE;
    }

    for (const std::string& name : s->screenshots) {
        if (!input::save_screenshot(iris, name)) {
            iris_error(&iris->log.script, "Couldn't save screenshot \"{}\"", name);
        }
    }

    s->screenshots.clear();

    if (!s->quit) {
        return SDL_APP_CONTINUE;
    }

    return s->quit_ok ? SDL_APP_SUCCESS : SDL_APP_FAILURE;
}

}
