#pragma once
#include <cstdint>
namespace fc::law11 {
struct Stats {bool installed{};std::uint64_t gold{},prison{},fine{};};
bool Install();
void SetPolicy(bool noBounty,bool noArrest);
bool TakeClearRequest();
Stats Snapshot();
}
