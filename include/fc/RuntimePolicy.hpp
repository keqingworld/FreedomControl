#pragma once
// Engine-independent decisions used by Engine.cpp and exercised by runtime_tests.cpp.
#include "fc/Core.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace fc {
// Own only the pause we acquired. A pre-existing pause is never unconditionally cleared.
class PauseLease {
    bool held_{}, before_{};
public:
    std::optional<bool> Update(bool wanted, bool observed) {
        if (wanted && !held_) { before_ = observed; held_ = true; return true; }
        if (wanted && held_ && !observed) return true;
        if (!wanted && held_) { held_ = false; return before_; }
        return std::nullopt;
    }
    bool Held() const { return held_; }
};
// Own the master audio volume while the F8 editor is open.  This is a
// deliberately tiny policy object so exact capture/restore semantics can be
// exercised without Skyrim.  A pre-muted game stays muted after closing.
class AudioMuteLease16 {
    bool held_{};
    float before_{1.0f};
public:
    std::optional<float> Update(bool wanted, float observed) {
        if (!std::isfinite(observed)) return std::nullopt;
        if (wanted && !held_) { before_=observed; held_=true; return 0.0f; }
        if (wanted && held_ && std::abs(observed)>0.0001f) return 0.0f;
        if (!wanted && held_) { held_=false; return before_; }
        return std::nullopt;
    }
    bool Held() const { return held_; }
    float Before() const { return before_; }
};
// No duplicate show/hide while a message is pending in the same UI cycle.
// Always append an inverse request to cancel a not-yet-observed show/hide.
inline bool NativePauseMessageNeeded15(bool wanted,bool onStack,std::optional<bool> pending) {
    return pending ? *pending!=wanted : wanted!=onStack;
}
inline bool CanFinishClose(bool requested, std::size_t queued, std::size_t batch) {
    return requested && queued == 0 && batch == 0;
}
inline int MissingQuantity(int requested, int owned, bool onlyMissing) {
    requested = std::clamp(requested, 1, 1000000);
    return onlyMissing ? std::max(0, requested - std::clamp(owned, 0, 1000000000)) : requested;
}
inline float DesiredBaseValue(float before,float baseBefore,float desired,bool effective) {
    if (!std::isfinite(before) || !std::isfinite(baseBefore) || !std::isfinite(desired)) return baseBefore;
    const double result=effective?static_cast<double>(baseBefore)+desired-before:desired;
    return static_cast<float>(std::clamp(result,-1.0e12,1.0e12));
}
// Work in double during boost arithmetic; no clamp with an inverted lower/upper bound.
inline float BoostedBase(float original,float maximum,float limit) {
    if (!std::isfinite(original) || !std::isfinite(maximum) || !std::isfinite(limit)) return original;
    const double result=static_cast<double>(original)+std::max(0.0,static_cast<double>(limit)-maximum);
    return static_cast<float>(std::clamp(result,-1.0e12,1.0e12));
}
inline float RestoreBoostedBase(float current,float original,float applied) {
    if (!std::isfinite(current) || !std::isfinite(original) || !std::isfinite(applied)) return current;
    return static_cast<float>(std::clamp(static_cast<double>(current)-(static_cast<double>(applied)-original),-1.0e12,1.0e12));
}
struct FollowDecision {
    bool recover{};
    bool distant{};
    bool stalled{};
};
class FollowProgress {
    float idleSeconds_{};
    std::array<float,3> previous_{};
    bool primed_{};
public:
    void Reset() { idleSeconds_=0; primed_=false; }
    float StalledSeconds() const { return idleSeconds_; }
    FollowDecision Update(float dt, const std::array<float,3>& pos, float distance,
                          float desired, float leash, bool otherCell, bool active,
                          bool forced, float stallLimit) {
        if (!active || !std::isfinite(distance) || !std::all_of(pos.begin(),pos.end(),[](float v){return std::isfinite(v);})) { Reset(); return {}; }
        desired=Finite(desired,240,64,10000);
        leash=std::max(Finite(leash,4000,200,100000),desired+100);
        dt=Finite(dt,0,0,0.75f); // time spent in menus / loading is not simulation time
        if (otherCell || distance > leash) { Reset(); return {true,true,false}; }
        if (distance <= desired+100) { Reset(); return {}; }
        if (!primed_) { previous_=pos; primed_=true; return {}; }
        const float moved=Distance(pos,previous_); previous_=pos;
        if (moved < std::max(2.0f,dt*16.0f)) idleSeconds_+=dt;
        else idleSeconds_=0;
        const bool stuck=idleSeconds_>=Finite(stallLimit,2.5f,0.5f,15);
        if (forced || stuck) { Reset(); return {true,false,stuck}; }
        return {};
    }
};
struct PanelBounds { float width{},height{},left{},top{}; };
inline PanelBounds FitPanel(float width,float height) {
    width=Finite(width,1920,320,16384); height=Finite(height,1080,240,16384);
    const float w=std::min(width-24.0f,std::max(680.0f,width*0.92f));
    const float h=std::min(height-24.0f,std::max(500.0f,height*0.92f));
    return {w,h,(width-w)*0.5f,(height-h)*0.5f};
}
inline bool IsFinitePoint(const std::array<float,3>& p) {
    return std::isfinite(p[0]) && std::isfinite(p[1]) && std::isfinite(p[2]);
}
}
