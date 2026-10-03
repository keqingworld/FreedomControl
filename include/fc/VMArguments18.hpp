#pragma once
#include <RE/F/FunctionArguments.h>
#include <type_traits>
#include <utility>

namespace fc {
// VMARGS18: Papyrus arguments are VALUES, not C++ reference parameters.
// MakeFunctionArguments forwards the deduced types into a constrained class.
// An array element of type Actor* is an lvalue and otherwise becomes Actor*&.
// Take copies first, then pass those copies as rvalues. This does not std::move
// from the caller's strings, arrays or pointers, and preserves typed nulls.
template <class... Args>
[[nodiscard]] inline RE::BSScript::IFunctionArguments* MakeVMArguments18(Args... args)
{
    static_assert((!std::is_reference_v<Args> && ...),
        "VMARGS18: Papyrus argument storage must not contain C++ references");
    static_assert((RE::BSScript::is_return_convertible_v<Args> && ...),
        "VMARGS18: Unsupported Papyrus argument type; use an engine-supported value");
    return RE::MakeFunctionArguments<Args...>(std::move(args)...);
}
}  // namespace fc
