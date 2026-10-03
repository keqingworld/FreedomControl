#pragma once
#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace fc {
using ID = std::uint32_t;
std::optional<ID> ParseID(std::string_view text);
bool SafeCommand(std::string_view text);
bool Identifier(std::string_view text);
bool Contains(std::string_view text, std::string_view query);
std::string Hex(ID id);
float Finite(float value, float fallback, float low, float high);
float Distance(const std::array<float, 3>& a, const std::array<float, 3>& b);
std::array<float, 3> LocalOffset(float yaw, float forward, float right, float up);
}
