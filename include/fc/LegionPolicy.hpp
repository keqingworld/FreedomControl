#pragma once
#include "fc/RuntimePolicy.hpp"
#include <limits>
#include <span>

namespace fc {
inline constexpr std::size_t MaxLegionMembers=512;
inline constexpr int MaxSpawnBatch=256;
struct CombatProfile {
    float health{1000}, magicka{500}, stamina{500}, damage{2}, armor{300}, resist{25}, speed{100};
    bool refill{};
    void Normalize() {
        health=Finite(health,1000,1,1000000); magicka=Finite(magicka,500,0,1000000);
        stamina=Finite(stamina,500,1,1000000); damage=Finite(damage,2,0,1000);
        armor=Finite(armor,300,0,100000); resist=Finite(resist,25,-1000,100);
        speed=Finite(speed,100,1,1000);
    }
};
// Stable slot numbers, not FormID % 5: large squads do not share five teleport points.
inline std::array<float,3> FormationOffset(std::size_t slot,float angle,float follow,float spacing) {
    slot=std::min(slot,MaxLegionMembers-1);
    spacing=Finite(spacing,160,64,2000); follow=Finite(follow,240,64,10000);
    // Square-root growth instead of hundreds of rows stretching past the active area.
    const float radius=spacing*0.55f*std::sqrt(static_cast<float>(slot));
    const float theta=static_cast<float>(slot)*2.39996322972865f;
    return LocalOffset(Finite(angle,0,-100000,100000),
        -follow+radius*std::cos(theta),radius*std::sin(theta),0);
}
// A deterministic golden-angle disk. radius=0 intentionally means exactly the anchor.
inline std::array<float,3> SpawnOffset(int index,int total,float radius,float angle) {
    total=std::clamp(total,1,MaxSpawnBatch); index=std::clamp(index,0,total-1);
    radius=Finite(radius,300,0,10000); angle=Finite(angle,0,-100000,100000);
    const float r=radius*std::sqrt((static_cast<float>(index)+0.5f)/static_cast<float>(total));
    const float theta=angle+static_cast<float>(index)*2.39996322972865f;
    return {r*std::cos(theta),r*std::sin(theta),0};
}
struct ActorRecordFacts {
    bool leveled{}, unique{}, preset{}; ID race{}; std::string_view source,search;
};
struct ActorRecordQuery {
    int type{}; bool uniqueOnly{}, hidePresets{}, byRace{}; ID race{}; std::string_view source,query;
};
inline bool ActorRecordMatches(const ActorRecordFacts& e,const ActorRecordQuery& q) {
    if(q.type==1 && e.leveled) return false;
    if(q.type==2 && !e.leveled) return false;
    if(!q.source.empty() && e.source!=q.source) return false;
    if(q.byRace && (e.leveled || e.race!=q.race)) return false;
    if(q.uniqueOnly && !e.unique) return false;
    if(q.hidePresets && e.preset) return false;
    return Contains(e.search,q.query);
}
struct CombatCandidate {
    ID id{}; float distance{}; bool hostile{}, attacksAlly{}, explicitOrder{};
    bool dead{}, disabled{}, friendly{}, sameArea{true};
};
inline bool ValidEnemy(const CombatCandidate& c,float radius) {
    return c.id!=0 && c.id!=0x14 && !c.dead && !c.disabled && !c.friendly && c.sameArea &&
        std::isfinite(c.distance) && c.distance>=0 && c.distance<=Finite(radius,5000,256,20000) &&
        (c.hostile || c.attacksAlly || c.explicitOrder);
}
inline ID PickEnemy(std::span<const CombatCandidate> candidates,float radius) {
    ID best=0; int bestPriority=-1; float bestDistance=std::numeric_limits<float>::max();
    for (const auto& c:candidates) if (ValidEnemy(c,radius)) {
        const int priority=c.explicitOrder?3:(c.attacksAlly?2:1);
        if (priority>bestPriority || (priority==bestPriority &&
            (c.distance<bestDistance || (c.distance==bestDistance && (!best || c.id<best))))) {
            best=c.id; bestPriority=priority; bestDistance=c.distance;
        }
    }
    return best;
}
inline bool LegionCanFight(bool autoFight,bool peace,bool freeze,bool pacified,bool waiting) {
    return autoFight && !peace && !freeze && !pacified && !waiting;
}
struct ProgressDecision { bool reassert{}, recover{}; };
class GoalProgress {
    std::array<float,3> previous_{}, previousGoal_{};
    float noProgress_{};
    bool primed_{};
public:
    void Reset() { primed_=false; noProgress_=0; }
    float Seconds() const { return noProgress_; }
    ProgressDecision Update(float dt,const std::array<float,3>& pos,const std::array<float,3>& goal,
                            float tolerance,bool active,bool otherArea,bool farAway,bool tether,float recoverSeconds=5.0f) {
        if (!active || !IsFinitePoint(pos) || !IsFinitePoint(goal)) { Reset(); return {}; }
        dt=Finite(dt,0,0,0.75f); tolerance=Finite(tolerance,120,32,10000);
        if (otherArea || farAway) { Reset(); return {false,true}; }
        if (Distance(pos,goal)<=tolerance) { Reset(); return {}; }
        if (tether) { Reset(); return {false,true}; }
        if (!primed_) { previous_=pos; previousGoal_=goal; primed_=true; return {}; }
        // Measure progress toward the PREVIOUS goal: player motion does not penalize a
        // follower travelling in the correct direction, unlike a raw distance delta.
        const float progress=Distance(previous_,previousGoal_)-Distance(pos,previousGoal_);
        if (progress>std::max(1.0f,dt*8.0f)) noProgress_=0;
        else noProgress_+=dt;
        previous_=pos; previousGoal_=goal;
        if (noProgress_>=Finite(recoverSeconds,5,5,120)) { Reset(); return {true,true}; }
        return {noProgress_>=2.5f,false};
    }
};
enum class DeathDecision { Retry, Confirmed, Remove, Failed };
inline DeathDecision CheckDeath(bool dead,int attempts,bool fallback) {
    if (dead) return DeathDecision::Confirmed;
    if (attempts<3) return DeathDecision::Retry;
    return fallback?DeathDecision::Remove:DeathDecision::Failed;
}
// Save-backed removal targets only the requested reference and its original base.
inline bool MatchesRemoval(ID ref,ID expectedBase,ID observedBase) {
    return ref!=0 && ref!=0x14 && expectedBase!=0 && expectedBase==observedBase;
}
}
