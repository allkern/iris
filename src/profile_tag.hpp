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

#include <atomic>
#include <cstdint>

#ifdef _WIN32
extern "C" __declspec(dllimport) unsigned long __stdcall GetCurrentThreadId(void);
extern "C" __declspec(dllimport) void* __stdcall GetCurrentThread(void);
extern "C" __declspec(dllimport) long __stdcall SetThreadDescription(void* thread, const wchar_t* description);
#endif

namespace iris::profile {

enum Jit : int {
    JIT_NONE = 0,
    JIT_EE,
    JIT_IOP,
    JIT_VU0,
    JIT_VU1
};

inline constexpr int JIT_SLOT_COUNT = 16;

struct JitSlot {
    std::atomic <uint32_t> thread_id = 0;
    volatile int jit = JIT_NONE;
};

inline JitSlot jit_slots[JIT_SLOT_COUNT];
inline JitSlot jit_slot_overflow;

inline uint32_t current_thread_id() {
#ifdef _WIN32
    return GetCurrentThreadId();
#else
    return 0;
#endif
}

inline void name_this_thread(const wchar_t* name) {
#ifdef _WIN32
    SetThreadDescription(GetCurrentThread(), name);
#endif
}

inline JitSlot* claim_jit_slot() {
    uint32_t id = current_thread_id();

    for (JitSlot& slot : jit_slots) {
        if (slot.thread_id.load() == id) {
            return &slot;
        }
    }

    for (JitSlot& slot : jit_slots) {
        uint32_t expected = 0;

        if (slot.thread_id.compare_exchange_strong(expected, id)) {
            return &slot;
        }
    }

    return &jit_slot_overflow;
}

struct JitSlotOwner {
    JitSlot* slot = nullptr;

    ~JitSlotOwner() {
        if (!slot || slot == &jit_slot_overflow) {
            return;
        }

        slot->jit = JIT_NONE;
        slot->thread_id.store(0);
    }
};

inline thread_local JitSlotOwner jit_slot_owner;

inline JitSlot* this_thread_jit_slot() {
    if (!jit_slot_owner.slot) {
        jit_slot_owner.slot = claim_jit_slot();
    }

    return jit_slot_owner.slot;
}

inline void set_active_jit(int jit) {
    this_thread_jit_slot()->jit = jit;
}

inline int get_active_jit() {
    return this_thread_jit_slot()->jit;
}

inline int find_active_jit(uint32_t thread_id) {
    for (JitSlot& slot : jit_slots) {
        if (slot.thread_id.load() == thread_id) {
            return slot.jit;
        }
    }

    return JIT_NONE;
}

}
