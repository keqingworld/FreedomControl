#include "PCH.h"
#include <RE/B/BGSDefaultObjectManager.h>
#include <RE/B/BGSSoundCategory.h>
#include <type_traits>
#include <utility>

#ifdef GetObject
#    error "COMPILE17: Windows GetObject macro leaked past PCH.h"
#endif

namespace {
using MasterSlot = decltype(std::declval<RE::BGSDefaultObjectManager&>()
    .GetObject<RE::BGSSoundCategory>(RE::DefaultObjectID::kMasterSoundCategory));
static_assert(std::is_same_v<MasterSlot, RE::BGSSoundCategory**>,
    "COMPILE17: DefaultObjectID lookup must return a category pointer slot");
static_assert(std::is_same_v<decltype(std::declval<RE::BGSSoundCategory&>()
    .GetCategoryVolume()), float>);
static_assert(std::is_same_v<decltype(std::declval<RE::BGSSoundCategory&>()
    .SetCategoryVolume(0.0f)), void>);
static_assert(std::is_same_v<decltype(std::declval<RE::PlayerCharacter&>()
    .GetFormID()), fc::ID>);
}
