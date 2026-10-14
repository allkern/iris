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

#include <asmjit/ujit.h>

namespace iris {

static inline asmjit::InvokeNode* jit_invoke(asmjit::ujit::UniCompiler& uc, uintptr_t func, const asmjit::FuncSignature& sig) {
    asmjit::InvokeNode* node = nullptr;

#if defined(ASMJIT_UJIT_AARCH64)
    // Note: ASMJIT bug workaround
    //       ASMJIT's AArch64 calling convention marks LR as preserved,
    //       but BLR overwrites it before the callee can save it.
    //       Mark it as unavailable to prevent ASMJIT from corrupting it.
    uc.cc->func()->frame().add_unavailable_regs(asmjit::a64::x30);

    asmjit::ujit::Gp target = uc.new_gp_ptr();

    uc.mov(target, asmjit::Imm(func));
    uc.cc->invoke(asmjit::Out(node), target, sig);
#else
    uc.cc->invoke(asmjit::Out(node), asmjit::Imm(func), sig);
#endif

    return node;
}

}
