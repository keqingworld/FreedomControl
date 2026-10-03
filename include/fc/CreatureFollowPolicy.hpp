#pragma once
#include <cstdint>

namespace fc {
// Capabilities, not dialogue availability, decide navigation recovery. Never
// change a shared race's flags to make a silent creature look like a human NPC.
struct CreatureLocomotion14 {
    bool known{}, walks{}, swims{}, flies{}, immobile{}, dragon{};
    constexpr bool AquaticOnly() const { return known && swims && !walks && !flies; }
    constexpr bool CanSnapToGround(bool swimming) const {
        return !dragon && !flies && !immobile && !swimming && (!known || walks);
    }
    constexpr bool CanRecoverNearPlayer(bool playerSwimming,bool playerInMidair) const {
        if(AquaticOnly())return playerSwimming;
        if(known && playerSwimming && !swims && !flies && !dragon)return false;
        return flies || dragon || !playerInMidair;
    }
};
// A package retry must not become a per-frame interruption loop. Existing quest
// aliases are tried first; repeated absence enables the unconditional authored
// fallback even if quest startup or alias binding itself failed.
constexpr bool CanForceFollowPackage14(bool force,bool frozen,bool waiting,bool combat,
    bool loaded,bool furniture,std::uint32_t attempts) {
    return force && !frozen && !waiting && !combat && loaded && !furniture && attempts>=2;
}
constexpr std::uint32_t NextFollowAttempt14(std::uint32_t attempts) {
    return attempts<3?attempts+1:3;
}
}
