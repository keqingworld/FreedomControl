#pragma once
// TEST-ONLY MODEL. Not CommonLib or Skyrim. Do not include in the game DLL.
// RE::Actor and RE::BSFixedString are provided by the caller's explicit doubles.
// The important constraint mirrors CommonLib: its supported argument types must
// be non-reference, non-const values. Engine allocation and VM handles are NOT
// modeled. std::any is used only to check copied values in a host process.
#include <any>
#include <cstdint>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>
namespace RE {
template<class T> using BSScrapArray = std::vector<T>;
namespace BSScript {
template<class T> struct is_return_convertible : std::bool_constant<
    !std::is_reference_v<T> && !std::is_const_v<T> && !std::is_volatile_v<T> &&
    (std::is_same_v<T, RE::Actor*> || std::is_same_v<T, RE::BSFixedString> ||
     std::is_same_v<T, std::int32_t> || std::is_same_v<T, std::uint32_t> ||
     std::is_same_v<T, float> || std::is_same_v<T, bool>)> {};
template<class T> inline constexpr bool is_return_convertible_v = is_return_convertible<T>::value;
struct Variable {
    std::any value;
    template<class T> void Pack(T item) { value = std::move(item); }
};
struct IFunctionArguments {
    virtual ~IFunctionArguments() = default;
    virtual bool operator()(BSScrapArray<Variable>& out) const = 0;
};
struct ZeroFunctionArguments final : IFunctionArguments {
    bool operator()(BSScrapArray<Variable>& out) const override { out.clear(); return true; }
};
// Deliberately incomplete for disallowed argument packs, as in CommonLib.
template<class Enable, class... Args> struct FunctionArguments;
template<class... Args>
struct FunctionArguments<std::enable_if_t<std::conjunction_v<is_return_convertible<Args>...>>, Args...>
    final : IFunctionArguments {
    std::tuple<std::decay_t<Args>...> values;
    explicit FunctionArguments(Args&&... args) : values(std::forward<Args>(args)...) {}
    bool operator()(BSScrapArray<Variable>& out) const override {
        out.clear(); out.reserve(sizeof...(Args));
        std::apply([&](const auto&... item) {
            (out.push_back(Variable{std::any(item)}), ...);
        }, values);
        return true;
    }
};
}
template<class... Args> using FunctionArguments = BSScript::FunctionArguments<void, Args...>;
template<class... Args> inline BSScript::IFunctionArguments* MakeFunctionArguments(Args&&... args) {
    return new FunctionArguments<Args...>(std::forward<Args>(args)...);
}
template<> inline BSScript::IFunctionArguments* MakeFunctionArguments() {
    return new BSScript::ZeroFunctionArguments();
}
}
