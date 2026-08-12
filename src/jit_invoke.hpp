#pragma once

#include <asmjit/ujit.h>

namespace iris {

static inline asmjit::InvokeNode* jit_invoke(asmjit::ujit::UniCompiler& uc, uintptr_t func, const asmjit::FuncSignature& sig) {
    asmjit::InvokeNode* node = nullptr;

#if defined(ASMJIT_UJIT_AARCH64)
    asmjit::ujit::Gp target = uc.new_gp_ptr();

    uc.mov(target, asmjit::Imm(func));
    uc.cc->invoke(asmjit::Out(node), target, sig);
#else
    uc.cc->invoke(asmjit::Out(node), asmjit::Imm(func), sig);
#endif

    return node;
}

}
