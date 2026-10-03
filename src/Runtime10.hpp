#pragma once
#include "PCH.h"
#include "fc/CreatureFollowPolicy.hpp"
namespace fc::runtime10 {
inline constexpr std::string_view File="FreedomControlRuntime.esp";
inline constexpr RE::FormID Quest=0x800,Members=0x801,Enemies=0x802,Dragons=0x803,Band=0x804,
    Cell=0x805,Parking=0x806,Effect=0x807,Spell=0x808,Orbit=0x809,Fallback=0x80A;
template<class T> T* Form(RE::FormID local) {
    auto* data=RE::TESDataHandler::GetSingleton();return data?data->LookupForm<T>(local,File):nullptr;
}
bool IsOwnFaction(RE::TESFaction* f);
bool IsOwnPackage(RE::TESPackage* p);
RE::TESPackage* FollowPackage(RE::Actor* actor,int slot,float distance);
float FollowTolerance(RE::Actor* actor,float distance,int slot);
CreatureLocomotion14 Locomotion(RE::Actor* actor);
bool CanSnapToGround(RE::Actor* actor);
bool CanRecoverNearPlayer(RE::Actor* actor,RE::Actor* player);
void LogFollowFailure(RE::Actor* actor,RE::TESPackage* wanted,int slot,std::string_view reason);
void InstallEvents();
}
