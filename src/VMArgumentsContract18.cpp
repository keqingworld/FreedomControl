#include "PCH.h"
#include "fc/VMArguments18.hpp"
#include <array>
#include <type_traits>

namespace {
static_assert(RE::BSScript::is_return_convertible_v<RE::Actor*>);
static_assert(RE::BSScript::is_return_convertible_v<RE::BSFixedString>);

// Compiled with the REAL CommonLib headers as part of the DLL target.
// These functions are NOT executed or exported. Their bodies instantiate the
// full argument templates, unlike a decltype-only signature check.
[[maybe_unused]] RE::BSScript::IFunctionArguments* CheckEightValueArguments18(
    const std::array<RE::Actor*, 5>& actors,
    const RE::BSFixedString& channel,
    const RE::BSFixedString& filter)
{
    return fc::MakeVMArguments18(
        actors[0], actors[1], actors[2], actors[3], actors[4],
        static_cast<RE::Actor*>(nullptr), channel, filter);
}

[[maybe_unused]] RE::BSScript::IFunctionArguments* CheckEmptyArguments18()
{
    return fc::MakeVMArguments18();
}
}  // namespace
