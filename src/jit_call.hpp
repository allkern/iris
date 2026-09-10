#pragma once

#include <tuple>
#include <type_traits>
#include <utility>

#include <asmjit/ujit.h>

#include "jit_invoke.hpp"

namespace iris {

template <typename F, typename = void>
struct jit_function_traits;

template <typename R, typename... A>
struct jit_function_traits<R(A...)> {
    using return_type = R;
    using args_type = std::tuple<A...>;
};

template <typename R, typename... A>
struct jit_function_traits<R(*)(A...)> {
    using return_type = R;
    using args_type = std::tuple<A...>;
};

template <class R, class... A>
static inline asmjit::FuncSignature jit_build_signature(R(*)(A...)) {
    return asmjit::FuncSignature::build<R, A...>();
}

template <class Func, class... Args, std::size_t... I>
static inline void jit_function_call_impl(asmjit::ujit::UniCompiler* uc, Func func, std::index_sequence<I...>, Args&&... args) {
    using R = typename jit_function_traits<Func>::return_type;

    asmjit::InvokeNode* call = jit_invoke(*uc, (uintptr_t)func, jit_build_signature(func));

    auto args_tuple = std::forward_as_tuple(std::forward<Args>(args)...);

    if constexpr (std::is_same_v<R, void>) {
        (call->set_arg(I, std::get<I>(args_tuple)), ...);
    } else {
        call->set_ret(0, std::get<0>(args_tuple));

        (call->set_arg(I, std::get<I + 1>(args_tuple)), ...);
    }
}

template <class Func, class... Args>
static inline void jit_function_call(asmjit::ujit::UniCompiler* uc, Func func, Args&&... args) {
    using R = typename jit_function_traits<Func>::return_type;

    constexpr std::size_t arg_count =
        std::is_same_v<R, void> ? sizeof...(Args) : sizeof...(Args) - 1;

    jit_function_call_impl(uc, func, std::make_index_sequence<arg_count>{}, std::forward<Args>(args)...);
}

}
