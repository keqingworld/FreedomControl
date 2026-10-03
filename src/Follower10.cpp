#include "PCH.h"
#include "Engine.hpp"
#include "Runtime10.hpp"
#include <RE/T/TESGlobal.h>

namespace fc::runtime10 {
bool IsOwnFaction(RE::TESFaction* f) {
    return f && (f==Form<RE::TESFaction>(Members)||f==Form<RE::TESFaction>(Enemies)||f==Form<RE::TESFaction>(Dragons));
}
bool IsOwnPackage(RE::TESPackage* p) {
    if(!p)return false;
    const auto* file=p->GetFile(0);
    if(!file || _stricmp(file->fileName,File.data())!=0)return false;
    const auto local=p->GetFormID()&0xFFF;
    return local==Orbit || local==Fallback || (local>=0x820 && local<0x838);
}
RE::TESPackage* FollowPackage(RE::Actor* actor,int slot,float distance) {
    if(!actor)return nullptr;
    if(actor->IsDragon())return Form<RE::TESPackage>(Orbit);
    const auto lane=static_cast<RE::FormID>(slot>=0?slot%3:0);
    auto* package=Form<RE::TESPackage>(0x820+static_cast<RE::FormID>(FollowBand10(distance))*3+lane);
    return package?package:Form<RE::TESPackage>(Fallback);
}
float FollowTolerance(RE::Actor* actor,float distance,int slot) {
    if(actor)if(auto* package=actor->GetCurrentPackage();IsOwnPackage(package)) {
        const auto local=package->GetFormID()&0xFFF;
        if(local==Orbit)return 4200.0f;
        if(local==Fallback)return 240.0f+192.0f;
        if(local>=0x820 && local<0x838) {
            const auto index=local-0x820;
            return FollowBands10[index/3]+static_cast<float>(index%3)*48.0f+192.0f;
        }
    }
    return actor && actor->IsDragon()?4200.0f:
        FollowBands10[FollowBand10(distance)]+static_cast<float>(slot>=0?slot%3:0)*48.0f+192.0f;
}
CreatureLocomotion14 Locomotion(RE::Actor* actor) {
    CreatureLocomotion14 out;
    if(!actor)return out;
    out.dragon=actor->IsDragon();
    if(const auto* race=actor->GetRace()) {
        using F=RE::RACE_DATA::Flag;
        out.known=true;out.walks=race->data.flags.any(F::kWalks);
        out.swims=race->data.flags.any(F::kSwims);out.flies=race->data.flags.any(F::kFlies);
        out.immobile=race->data.flags.any(F::kImmobile);
    }
    return out;
}
bool CanSnapToGround(RE::Actor* actor) {
    return actor && Locomotion(actor).CanSnapToGround(actor->IsSwimming());
}
void LogFollowFailure(RE::Actor* actor,RE::TESPackage* wanted,int slot,std::string_view reason) {
    if(!actor)return;
    const auto* base=actor->GetActorBase();const auto* race=actor->GetRace();
    const auto* actual=actor->GetCurrentPackage();const auto move=Locomotion(actor);
    spdlog::warn("Freedom14 follow {}: ref={:08X}, base={:08X}, race={:08X}, actual={:08X}, wanted={:08X}, alias={}, AI={}, 3D={}, process={}, walks={}, swims={}, flies={}, immobile={}, dragon={}",
        reason,actor->GetFormID(),base?base->GetFormID():0,race?race->GetFormID():0,
        actual?actual->GetFormID():0,wanted?wanted->GetFormID():0,slot,actor->IsAIEnabled(),
        actor->Get3D()!=nullptr,actor->GetActorRuntimeData().currentProcess!=nullptr,
        move.walks,move.swims,move.flies,move.immobile,move.dragon);
}
bool CanRecoverNearPlayer(RE::Actor* actor,RE::Actor* player) {
    return actor && player && Locomotion(actor).CanRecoverNearPlayer(player->IsSwimming(),player->IsInMidair());
}
}
namespace fc {
bool Engine::EnsureRuntime10() {
    auto* quest=runtime10::Form<RE::TESQuest>(runtime10::Quest);
    auto* parking=runtime10::Form<RE::TESObjectREFR>(runtime10::Parking);
    runtimeReady10_=quest && parking && quest->aliases.size()==MaxLegionMembers &&
        runtime10::Form<RE::TESFaction>(runtime10::Members) && runtime10::Form<RE::TESPackage>(runtime10::Fallback);
    if(!runtimeReady10_)return false;
    const auto now=std::chrono::steady_clock::now();
    if(!quest->IsEnabled() || !quest->IsRunning()) {
        if(now-questAttempt10_>std::chrono::seconds(2)) {
            questAttempt10_=now;
            const bool accepted=quest->Start();
            spdlog::info("Freedom10: independent quest Start={}, enabled={}, running={}",accepted,quest->IsEnabled(),quest->IsRunning());
        }
        if(!quest->IsEnabled() || !quest->IsRunning())return false;
    }
    // One reconciliation per loaded save. Only touch aliases owned by OUR quest.
    // Parking replaces an old reference using the engine's normal alias machinery.
    if(!aliasReconciled10_) {
        slots10_.Reset();
        for(std::uint32_t i=0;i<MaxLegionMembers;++i) {
            auto previous=quest->GetAliasedRef(i).get();
            if(previous && previous.get()!=parking)quest->ForceRefIntoAlias(i,parking);
        }
        for(auto& [id,state]:followers_) {state.aliasSlot=-1;state.aliasRefreshPending15=false;state.package=nullptr;state.retrySeconds=0;state.packageAttempts14=0;state.recoveryCooldown14=0;state.packageLogCooldown14=0;}
        aliasReconciled10_=true;
        spdlog::info("Freedom10: authored quest ready; {} independent reference aliases",quest->aliases.size());
    }
    return true;
}
bool Engine::BindFollower10(RE::Actor* actor,FollowState& state) {
    if(!actor || actor->GetFormID()==0x14 || actor->IsDeleted() || actor->IsDisabled() || actor->IsDead(false))return false;
    if(!EnsureRuntime10()) {
        state.status=runtimeReady10_?"独立任务启动中":"缺少 FreedomControlRuntime.esp：请勾选右侧插件";
        return false;
    }
    auto* quest=runtime10::Form<RE::TESQuest>(runtime10::Quest);
    const int slot=slots10_.Acquire(state.id);
    if(slot<0) {state.status="独立随从别名已满";return false;}
    state.aliasSlot=slot;
    const auto previous=quest->GetAliasedRef(static_cast<std::uint32_t>(slot)).get();
    // Avoid continuously dirtying faction state when the same alias is already bound.
    if(previous.get()!=actor || state.controlTimer>=1.99f) {
    if(auto* faction=runtime10::Form<RE::TESFaction>(runtime10::Members))
        actor->AddToFaction(faction,static_cast<std::int8_t>(slot%3));
    if(auto* enemy=runtime10::Form<RE::TESFaction>(runtime10::Enemies))actor->RemoveFromFaction(enemy);
    enemies10_.erase(state.id);
    if(auto* dragon=runtime10::Form<RE::TESFaction>(runtime10::Dragons)) {
        if(actor->IsDragon())actor->AddToFaction(dragon,0);else actor->RemoveFromFaction(dragon);
    }
    }
    if(auto* band=runtime10::Form<RE::TESGlobal>(runtime10::Band))band->value=static_cast<float>(FollowBand10(followDistance_));
    if(previous.get()!=actor) {
        quest->ForceRefIntoAlias(static_cast<std::uint32_t>(slot),actor);
        const auto actual=quest->GetAliasedRef(static_cast<std::uint32_t>(slot)).get();
        if(actual.get()!=actor) {
            slots10_.Release(state.id);state.aliasSlot=-1;
            state.status="别名填充读回失败（未伪报接管成功）";
            if(state.packageLogCooldown14<=0) {
                runtime10::LogFollowFailure(actor,runtime10::FollowPackage(actor,slot,followDistance_),slot,"alias binding rejected");
                state.packageLogCooldown14=10;
            }
            return false;
        }
        state.aliasRefreshPending15=true;
        spdlog::info("Freedom10 {:08X}: independently bound alias {}; one selection refresh pending",state.id,slot);
    }
    auto* pack=actor->GetCurrentPackage();state.package=runtime10::IsOwnPackage(pack)?pack:nullptr;
    state.status=state.package?"独立跟随包已读回（移动待验证）":"别名已绑定；等待引擎执行跟随程序";
    return true;
}
void Engine::ReleaseFollower10(ID id) {
    auto* quest=runtime10::Form<RE::TESQuest>(runtime10::Quest);
    auto* parking=runtime10::Form<RE::TESObjectREFR>(runtime10::Parking);
    if(quest && parking) {
        // Find all actual bindings, not just the cached slot (also repairs older saves).
        const auto count=std::min<std::size_t>(quest->aliases.size(),MaxLegionMembers);
        for(std::uint32_t i=0;i<count;++i) {
            auto ref=quest->GetAliasedRef(i).get();
            if(ref && ref->GetFormID()==id)quest->ForceRefIntoAlias(i,parking);
        }
    }
    if(auto* ref=Ref(id))if(auto* actor=ref->As<RE::Actor>())
        for(ID local:{runtime10::Members,runtime10::Dragons})
            if(auto* f=runtime10::Form<RE::TESFaction>(local))actor->RemoveFromFaction(f);
    slots10_.Release(id);
}
void Engine::ResetRuntime10() {
    slots10_.Reset();aliasReconciled10_=false;questAttempt10_={};births10_.clear();requestedForms10_.clear();
    enemies10_.clear();clearJobs10_.clear();clearQueued10_.clear();removalRing10_.clear();
    clearDone10_=clearTotal10_=spawnVisible10_=spawnFollowReady14_=spawnFollowPending14_=0; aura10_=emptyWorld10_=false;protectArmy10_=true;
    clearRadius10_=2000;stuckDelay10_=12;freedomTimer10_=0;
}
}
