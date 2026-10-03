#pragma once
#include <algorithm>
#include <cmath>
namespace fc::custom12 {
inline bool ValidMaximum(float v) { return std::isfinite(v) && v >= 1 && v <= 1000000; }
inline bool ValidPercent(float v) { return std::isfinite(v) && v >= 0 && v <= 100; }
inline bool ValidLevel(int v) { return v >= 1 && v <= 65535; }
inline float Fraction(float current, float maximum) {
    return std::isfinite(current) && std::isfinite(maximum) && maximum > 0
        ? std::clamp(current / maximum, 0.0f, 1.0f) : 1.0f;
}
// Use maximum, never damaged current, when compensating equipment/effect modifiers.
inline float BaseForMaximum(float base, float maximum, float desired) {
    return base + desired - maximum;
}
inline float DesiredCurrent(float maximum, float fraction, bool health) {
    return std::clamp(maximum * fraction, health ? std::min(1.0f, maximum) : 0.0f, maximum);
}
// Positive damage modifier restores resources; negative modifier consumes them.
// No base/permanent/temporary modifiers are touched by a current-value edit.
template<class Actor, class Value, class Modifier>
bool SetCurrent(Actor& actor, Value av, Modifier damage, float fraction, bool health) {
    const float maximum=actor.GetActorValueMax(av), current=actor.GetActorValue(av);
    if (!std::isfinite(maximum) || maximum <= 0 || !std::isfinite(current) || !std::isfinite(fraction)) return false;
    const float desired=DesiredCurrent(maximum,std::clamp(fraction,0.0f,1.0f),health);
    actor.ModActorValue(damage,av,desired-current);
    return true;
}
}
