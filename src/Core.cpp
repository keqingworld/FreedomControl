#include "fc/Core.hpp"
#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdio>

namespace fc {
std::optional<ID> ParseID(std::string_view text) {
    while (!text.empty() && (text.front() == ' ' || text.front() == '\t')) text.remove_prefix(1);
    while (!text.empty() && (text.back() == ' ' || text.back() == '\t')) text.remove_suffix(1);
    if (text.starts_with("0x") || text.starts_with("0X")) text.remove_prefix(2);
    if (text.empty() || text.size() > 8) return {};
    ID result{};
    auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), result, 16);
    if (error != std::errc{} || end != text.data() + text.size()) return {};
    return result;
}
bool SafeCommand(std::string_view s) {
    return !s.empty() && s.size() <= 512 && std::none_of(s.begin(), s.end(), [](unsigned char c) {
        return c == 0 || c == '\r' || c == '\n';
    });
}
bool Identifier(std::string_view s) {
    return !s.empty() && s.size() <= 96 && std::all_of(s.begin(), s.end(), [](unsigned char c) {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
               (c >= '0' && c <= '9') || c == '_';
    });
}
bool Contains(std::string_view text, std::string_view query) {
    auto lower = [](unsigned char c) { return c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c; };
    return std::search(text.begin(), text.end(), query.begin(), query.end(),
        [&](char a, char b) { return lower(static_cast<unsigned char>(a)) == lower(static_cast<unsigned char>(b)); }) != text.end();
}
std::string Hex(ID id) {
    std::array<char, 9> buffer{};
    std::snprintf(buffer.data(), buffer.size(), "%08X", id);
    return buffer.data();
}
float Finite(float value, float fallback, float low, float high) {
    return std::clamp(std::isfinite(value) ? value : fallback, low, high);
}
float Distance(const std::array<float, 3>& a, const std::array<float, 3>& b) {
    return std::hypot(a[0] - b[0], a[1] - b[1], a[2] - b[2]);
}
std::array<float, 3> LocalOffset(float yaw, float forward, float right, float up) {
    return {std::sin(yaw) * forward + std::cos(yaw) * right,
            std::cos(yaw) * forward - std::sin(yaw) * right, up};
}
}
