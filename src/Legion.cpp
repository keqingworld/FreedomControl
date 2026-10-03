#include "PCH.h"
#include "Engine.hpp"
#include "Runtime10.hpp"
#include <RE/B/BGSScene.h>
#include <RE/T/TESLevCharacter.h>

namespace fc {
namespace {
constexpr ID PlayerID=0x14;
std::array<float,3> Pos(const RE::NiPoint3& p) { return {p.x,p.y,p.z}; }
std::string Label(RE::TESObjectREFR* ref) {
    if (!ref) return "<unloaded>";
    const auto* name=ref->GetDisplayFullName();
    return name && *name ? name : "["+Hex(ref->GetFormID())+"]";
}
bool SameArea(RE::TESObjectREFR* a,RE::TESObjectREFR* b) {
    if(!a || !b) return false;
    const auto* ca=a->GetParentCell(); const auto* cb=b->GetParentCell();
    if(!ca || !cb) return false;
    return ca==cb || (ca->IsExteriorCell() && cb->IsExteriorCell() && a->GetWorldspace()==b->GetWorldspace());
}
constexpr RE::Actor::BOOL_FLAGS ControlFlags[]={RE::Actor::BOOL_FLAGS::kMovementBlocked,
    RE::Actor::BOOL_FLAGS::kAttackingDisabled,RE::Actor::BOOL_FLAGS::kCastingDisabled};
constexpr RE::Actor::BOOL_BITS ControlBits[]={RE::Actor::BOOL_BITS::kHeadingFixed,RE::Actor::BOOL_BITS::kParalyzed};
}

bool Engine::IsLegionFriendly(RE::Actor* actor) const {
    return actor && (actor->GetFormID()==PlayerID || followers_.contains(actor->GetFormID()) || actor->IsPlayerTeammate() ||
        std::any_of(births10_.begin(),births10_.end(),[&](const Birth10& birth){return birth.recruit && birth.id==actor->GetFormID() && birth.epoch==epoch_.load();}));
}
void Engine::IsolateActorFactions(RE::Actor* actor,FollowState& state) {
    if(!actor || !isolateFactions_) return;
    // VisitFactions includes base entries followed by reference overrides. Last wins.
    // Never edit the container while its visitor is executing.
    std::unordered_map<ID,int> current;
    actor->VisitFactions([&](RE::TESFaction* faction,std::int8_t rank) {
        if(faction && !runtime10::IsOwnFaction(faction)) current[faction->GetFormID()]=rank;
        return false;
    });
    bool changed=false;
    for(const auto& [id,rank]:current) {
        // Delayed OnInit/OnLoad scripts can re-add factions after recruitment.
        // Preserve the first observed rank; never overwrite it with our -1 override.
        if(!state.oldFactions.contains(id) && state.oldFactions.size()>=2048)continue;
        state.oldFactions.try_emplace(id,rank);
        if(rank>=0) if(auto* faction=RE::TESForm::LookupByID<RE::TESFaction>(id)) {
            actor->RemoveFromFaction(faction); changed=true;
        }
    }
    state.isolated=true;
    if(changed) if(auto* processes=RE::ProcessLists::GetSingleton()) processes->ClearCachedFactionFightReactions();
}
void Engine::RestoreActorFactions(RE::Actor* actor,FollowState& state) {
    if(!actor || !state.isolated) return;
    for(const auto& [id,rank]:state.oldFactions) if(auto* faction=RE::TESForm::LookupByID<RE::TESFaction>(id)) {
        if(rank<0) actor->RemoveFromFaction(faction);
        else actor->AddToFaction(faction,static_cast<std::int8_t>(std::clamp(rank,0,127)));
    }
    state.oldFactions.clear(); state.isolated=false;
    if(auto* processes=RE::ProcessLists::GetSingleton()) processes->ClearCachedFactionFightReactions();
}
void Engine::PrepareLegionActor(RE::Actor* actor,FollowState& state,bool interrupt) {
    if(!actor || actor->IsDead(false) || actor->IsDisabled() || actor->IsDeleted()) return;
    auto& data=actor->GetActorRuntimeData();
    if(!state.initialized9) {
        state.oldConfidence=actor->GetBaseActorValue(RE::ActorValue::kConfidence);
        state.oldAssistance=actor->GetBaseActorValue(RE::ActorValue::kAssistance);
        state.initialized9=true;
    }
    if(!state.controlsCaptured) {
        for(auto flag:ControlFlags) if(data.boolFlags.any(flag)) state.oldControlFlags|=static_cast<std::uint32_t>(flag);
        for(auto flag:ControlBits) if(data.boolBits.any(flag)) state.oldControlBits|=static_cast<std::uint32_t>(flag);
        state.controlsCaptured=true;
    }
    if(freeze_ || state.waiting) {state.controlTimer=0;return;}
    // Reference-local allegiance. No global faction alliance or shared NPC AI edit.
    if(!actor->IsPlayerTeammate()) Run("setplayerteammate 1",actor);
    actor->SetBaseActorValue(RE::ActorValue::kWaitingForPlayer,state.waiting?1.0f:0.0f);
    actor->SetBaseActorValue(RE::ActorValue::kAggression,0); // explicit enemy selection handles combat
    actor->SetBaseActorValue(RE::ActorValue::kConfidence,4);
    actor->SetBaseActorValue(RE::ActorValue::kAssistance,1);
    if(!actor->IsAIEnabled() && !state.waiting) actor->EnableAI(true);
    if(isolateFactions_) IsolateActorFactions(actor,state);
    if(hardControl_) {
        for(auto flag:ControlFlags) data.boolFlags.reset(flag);
        for(auto flag:ControlBits) data.boolBits.reset(flag);
        if(actor->GetLifeState()==RE::ACTOR_LIFE_STATE::kRestrained || actor->IsUnconscious())
            actor->SetLifeState(RE::ACTOR_LIFE_STATE::kAlive);
        if(auto* scene=actor->GetCurrentScene()) {
            const auto sceneID=scene->GetFormID();
            actor->StopCurrentDialogue();
            actor->SetCurrentScene(nullptr);
            data.boolFlags.reset(RE::Actor::BOOL_FLAGS::kScenePackage);
            spdlog::info("Legion9 {:08X}: detach actor from scene {:08X}; quest itself was not stopped",state.id,sceneID);
        }
        if(interrupt) actor->SetPlayerControls(false); // restore AI-driven movement, not puppet controls
    }
    if(interrupt && data.currentProcess) {
        if(actor->IsInCombat()) actor->StopCombat();
        actor->EndInterruptPackage(false); state.package=nullptr;
        if(actor->GetOccupiedFurniture().get() || actor->GetSitSleepState()!=RE::SIT_SLEEP_STATE::kNormal) {
            actor->InitiateGetUpPackage(); state.retrySeconds=1.5f;
            state.status="正在退出座椅 / 睡眠";
        }
    }
    state.controlTimer=2.0f;
}
void Engine::Follow(RE::Actor* actor) {
    if(!actor || actor->GetFormID()==PlayerID) { Note("请选择非玩家角色。"); return; }
    const ID id=actor->GetFormID();
    // A quest may start asynchronously, or another mod may prevent alias binding.
    // Neither is a reason to disable the independent actor-local fallback.
    if(!runtime10::Form<RE::TESPackage>(runtime10::Fallback)) {
        Note("缺少独立跟随程序。请启用配套 FreedomControlRuntime.esp。");return;
    }
    EnsureRuntime10();
    if(actor->IsDead(false) || actor->IsDeleted() || actor->IsDisabled() || removed_.contains(id)) {
        Note("目标已死亡或移除，不能直接招募。"); return;
    }
    if(!followers_.contains(id) && followers_.size()>=MaxLegionMembers) { Note("军团已达 512 个受控引用；请先解除部分成员。"); return; }
    auto [it,added]=followers_.try_emplace(id); auto& state=it->second;
    if(added) {
        state.id=id; state.oldAI=frozen_.contains(id)?frozen_.at(id):actor->IsAIEnabled();
        state.oldTeammate=actor->IsPlayerTeammate();
        state.oldAggression=pacified_.contains(id)?pacified_.at(id):actor->GetBaseActorValue(RE::ActorValue::kAggression);
        state.oldWaiting=actor->GetBaseActorValue(RE::ActorValue::kWaitingForPlayer);
        const auto movement=runtime10::Locomotion(actor);const auto* race=actor->GetRace();
        spdlog::info("Freedom14 recruit {:08X}: race={:08X}, walks={}, swims={}, flies={}, immobile={}, dragon={}, 3D={}, process={}",
            id,race?race->GetFormID():0,movement.walks,movement.swims,movement.flies,movement.immobile,movement.dragon,
            actor->Get3D()!=nullptr,actor->GetActorRuntimeData().currentProcess!=nullptr);
    }
    pacified_.erase(id); // recruitment replaces our older pacify order
    state.waiting=false; state.progress.Reset(); state.goalProgress.Reset(); state.retrySeconds=0;state.packageAttempts14=0;
    state.status="军团接管中";
    PrepareLegionActor(actor,state,true);
    SetFollowPackage(actor,state);
    dirty_=true;
    Note("已登记现有角色："+Label(actor)+"；这不是生成按钮。独立别名和当前行为包会持续读回。");
}
void Engine::SetFollowPackage(RE::Actor* actor,FollowState& state) {
    if(!actor || actor->IsDead(false) || actor->IsDisabled() || actor->IsDeleted())return;
    // GetCurrentPackage reads the running (including run-once) package; issuing
    // EvaluatePackage is not synchronous evidence that the engine selected it.
    auto* actual=actor->GetCurrentPackage();
    state.package=runtime10::IsOwnPackage(actual)?actual:nullptr;
    if(freeze_ || state.waiting || state.retrySeconds>0)return;
    auto observeOwn=[&]() {
        actual=actor->GetCurrentPackage();
        state.package=runtime10::IsOwnPackage(actual)?actual:nullptr;
        if(!state.package)return false;
        state.packageAttempts14=0;state.packageLogCooldown14=0;state.retrySeconds=2;
        state.status=(actual->GetFormID()&0xFFF)==runtime10::Fallback?
            "独立保底跟随包已读回；持续检查实际移动":"独立别名跟随包已读回；持续检查实际移动";
        return true;
    };
    const bool bound=BindFollower10(actor,state);
    const bool loaded=actor->GetActorRuntimeData().currentProcess && actor->Get3D();
    const bool furniture=actor->GetOccupiedFurniture().get() || actor->GetSitSleepState()!=RE::SIT_SLEEP_STATE::kNormal;
    if(actor->IsInCombat() || !loaded || furniture)return;
    // A newly acquired alias gets ONE chance to promote a temporary fallback
    // to its distance/dragon route. Keep this pending across combat/loading; a
    // repeated maintenance pass on the same binding never restarts the path.
    bool evaluated=false;
    if(bound && state.aliasRefreshPending15) {
        state.aliasRefreshPending15=false;
        auto* current=actor->GetCurrentPackage();
        if(runtime10::IsOwnPackage(current) && (current->GetFormID()&0xFFF)==runtime10::Fallback)
            actor->EndInterruptPackage(false);
        actor->EvaluatePackage(false,false);evaluated=true;
    }
    // Any authored follow selected by our independent alias is a valid route.
    // In particular, the unconditional fallback must not be cancelled every two
    // seconds merely because it differs from the distance-selector prediction.
    if(observeOwn())return;
    if(bound && !evaluated)actor->EvaluatePackage(false,false); // preserve an in-flight path / queued evaluation
    if(observeOwn())return;
    state.packageAttempts14=NextFollowAttempt14(state.packageAttempts14);
    // Separate escape path: no quest-start, alias, faction-rank or global-band
    // condition is a prerequisite. A real authored program remains required.
    auto* fallback=runtime10::Form<RE::TESPackage>(runtime10::Fallback);
    if(fallback && CanForceFollowPackage14(hardControl_ || forcedFollow_,freeze_,state.waiting,
        actor->IsInCombat(),loaded,furniture,state.packageAttempts14)) {
        actor->EndInterruptPackage(false);
        actor->PutCreatedPackage(fallback,true,false,false);
        if(state.packageAttempts14==2 || runtime10::IsOwnPackage(actor->GetCurrentPackage()))
            spdlog::info("Freedom15 {:08X}: unconditional follow override {:08X}, alias={}, observed={:08X}",
                state.id,fallback->GetFormID(),bound,actor->GetCurrentPackage()?actor->GetCurrentPackage()->GetFormID():0);
    }
    if(observeOwn())return;
    state.retrySeconds=2;
    state.status=bound?"跟随程序等待引擎执行；限频重试":"别名未就绪；独立临时跟随继续重试";
    if(state.packageLogCooldown14<=0) {
        runtime10::LogFollowFailure(actor,fallback,state.aliasSlot,bound?"follow not observed":"alias unavailable; fallback pending");
        state.packageLogCooldown14=10;
    }
}
void Engine::WaitFollower(ID id,bool waiting) {
    auto it=followers_.find(id);if(it==followers_.end())return;
    auto& state=it->second;
    state.waiting=waiting;state.progress.Reset();state.goalProgress.Reset();
    state.retrySeconds=0;state.packageAttempts14=0;
    dirty_=true;
    auto* ref=Ref(id);auto* actor=ref?ref->As<RE::Actor>():nullptr;
    if(!actor || actor->IsDead(false) || actor->IsDisabled() || actor->IsDeleted())return;
    actor->SetBaseActorValue(RE::ActorValue::kWaitingForPlayer,waiting?1.0f:0.0f);
    if(waiting && actor->IsInCombat())actor->StopCombat();
    // Apply immediately while the overlay pauses normal maintenance. An individual
    // Continue command must obey the same global-freeze precedence as army orders.
    actor->EnableAI(!freeze_ && !waiting);
    if(!freeze_ && !waiting) {
        state.package=nullptr;PrepareLegionActor(actor,state,true);SetFollowPackage(actor,state);
    }
}
void Engine::Dismiss(ID id) {
    // Cancel all remaining birth actions for this reference, including a delayed
    // profile write or navmesh move. Release stays authoritative even if initial
    // recruitment never acquired a member entry.
    if(std::erase_if(births10_,[id](const Birth10& birth){return birth.id==id;}))dirty_=true;
    auto it=followers_.find(id); if(it==followers_.end()) return;
    auto& state=it->second;
    ReleaseFollower10(id);
    if(auto* ref=Ref(id)) if(auto* actor=ref->As<RE::Actor>()) {
        if(runtime10::IsOwnPackage(actor->GetCurrentPackage())) actor->EndInterruptPackage(false);
        RestoreActorFactions(actor,state);
        actor->SetBaseActorValue(RE::ActorValue::kWaitingForPlayer,state.oldWaiting);
        actor->SetBaseActorValue(RE::ActorValue::kAggression,pacified_.contains(id)?0:state.oldAggression);
        if(state.initialized9) {
            actor->SetBaseActorValue(RE::ActorValue::kConfidence,state.oldConfidence);
            actor->SetBaseActorValue(RE::ActorValue::kAssistance,state.oldAssistance);
        }
        if(state.controlsCaptured) {
            auto& data=actor->GetActorRuntimeData();
            for(auto flag:ControlFlags) if(state.oldControlFlags & static_cast<std::uint32_t>(flag)) data.boolFlags.set(flag);
            for(auto flag:ControlBits) if(state.oldControlBits & static_cast<std::uint32_t>(flag)) data.boolBits.set(flag);
        }
        Run(state.oldTeammate?"setplayerteammate 1":"setplayerteammate 0",actor);
        if(!actor->IsDead(false) && !actor->IsDisabled()) {
            if(freeze_)frozen_[id]=state.oldAI;
            actor->EnableAI(!freeze_ && state.oldAI);
            if(!freeze_)actor->EvaluatePackage(true,true);
        }
    }
    followers_.erase(it); dirty_=true;
}
void Engine::SetMemberStats(RE::Actor* actor,CombatProfile profile,bool refillNow) {
    if(!actor || actor->GetFormID()==PlayerID || actor->IsDead(false)) return;
    profile.Normalize();
    const std::array<std::pair<RE::ActorValue,float>,3> pools{{
        {RE::ActorValue::kHealth,profile.health},{RE::ActorValue::kMagicka,profile.magicka},{RE::ActorValue::kStamina,profile.stamina}}};
    for(const auto& [av,wanted]:pools) {
        const float max=actor->GetActorValueMax(av);
        if(std::isfinite(max) && std::abs(max-wanted)>.1f) {
            const float base=actor->GetBaseActorValue(av);
            actor->SetBaseActorValue(av,DesiredBaseValue(max,base,wanted,true));
        }
        if(refillNow || profile.refill) {
            const float current=actor->GetActorValue(av), maximum=actor->GetActorValueMax(av);
            if(std::isfinite(current) && std::isfinite(maximum) && maximum>current) actor->RestoreActorValue(av,maximum-current);
        }
    }
    auto write=[&](RE::ActorValue av,float wanted) {
        const float before=actor->GetActorValue(av), base=actor->GetBaseActorValue(av);
        if(std::isfinite(before) && std::abs(before-wanted)>.01f) actor->SetBaseActorValue(av,DesiredBaseValue(before,base,wanted,true));
    };
    write(RE::ActorValue::kAttackDamageMult,profile.damage);
    write(RE::ActorValue::kDamageResist,profile.armor);
    write(RE::ActorValue::kResistMagic,profile.resist);
    if(std::abs(actor->GetActorValue(RE::ActorValue::kSpeedMult)-profile.speed)>.1f) SetValue(actor,"speedmult",profile.speed,true);
    dirty_=true;
}
void Engine::MaintainLegion(float dt,const std::vector<ID>& actors) {
    auto* player=RE::PlayerCharacter::GetSingleton(); if(!player || !player->GetParentCell()) return;
    std::vector<CombatCandidate> threats;
    threats.reserve(actors.size());
    if(autoFight_ && !peace_ && !freeze_) for(ID id:actors) {
        auto* ref=Ref(id); auto* enemy=ref?ref->As<RE::Actor>():nullptr;
        if(!enemy || IsLegionFriendly(enemy) || removed_.contains(id) || enemy->IsDead(false) || enemy->IsDisabled() || enemy->IsDeleted() || !SameArea(enemy,player)) continue;
        auto victim=enemy->GetActorRuntimeData().currentCombatTarget.get();
        CombatCandidate c{id,Distance(Pos(enemy->GetPosition()),Pos(player->GetPosition())),false,
            victim && IsLegionFriendly(victim.get()),id==focusEnemy_};
        if(c.distance>battleRadius_) continue;
        c.hostile=enemies10_.contains(id) || enemy->IsHostileToActor(player);
        if(ValidEnemy(c,battleRadius_)) threats.push_back(c);
    }
    if(focusEnemy_) {
        auto* ref=Ref(focusEnemy_); auto* enemy=ref?ref->As<RE::Actor>():nullptr;
        if(!enemy || enemy->IsDead(false) || enemy->IsDisabled() || IsLegionFriendly(enemy)) focusEnemy_=0;
    }
    std::vector<ID> ids; ids.reserve(followers_.size());
    for(const auto& [id,state]:followers_) ids.push_back(id);
    std::sort(ids.begin(),ids.end());
    std::size_t slot=0;
    for(ID id:ids) {
        auto it=followers_.find(id); if(it==followers_.end()) continue;
        auto& state=it->second;
        auto* ref=Ref(id); auto* actor=ref?ref->As<RE::Actor>():nullptr;
        const auto memberSlot=slot++;
        state.retrySeconds=std::max(0.0f,state.retrySeconds-dt);
        state.controlTimer=std::max(0.0f,state.controlTimer-dt);
        state.combatCooldown=std::max(0.0f,state.combatCooldown-dt);
        state.recoveryCooldown14=std::max(0.0f,state.recoveryCooldown14-dt);
        state.packageLogCooldown14=std::max(0.0f,state.packageLogCooldown14-dt);
        if(!actor || actor->IsDeleted() || actor->IsDisabled() || actor->IsDead(false)) { state.status="目标未加载 / 禁用 / 死亡"; state.goalProgress.Reset(); continue; }
        if(freeze_) { state.status="全局冻结优先"; state.goalProgress.Reset(); continue; }
        if(state.waiting) { if(actor->IsInCombat()) actor->StopCombat(); actor->EnableAI(false); state.status="等待命令"; state.goalProgress.Reset(); continue; }
        if(!state.initialized9 || state.controlTimer<=0) PrepareLegionActor(actor,state,false);
        if(!actor->IsAIEnabled()) actor->EnableAI(true);
        if(state.profileEnabled && state.controlTimer>=1.99f) SetMemberStats(actor,state.profile,false);
        if(actor->IsDragon() && !dragonsIndoors_ && player->GetParentCell()->IsInteriorCell()) { state.status="龙在室外待命"; state.goalProgress.Reset(); continue; }
        const bool otherArea=!SameArea(actor,player);
        const bool furniture=actor->GetOccupiedFurniture().get() || actor->GetSitSleepState()!=RE::SIT_SLEEP_STATE::kNormal;
        if(furniture && actor->GetActorRuntimeData().currentProcess) {
            state.furnitureSeconds+=dt;
            if(state.retrySeconds<=0) {
                if(hardControl_ && state.furnitureSeconds>=3.0f) {
                    actor->StopInteractingQuick(false);
                    actor->DoSetSitSleepState(RE::SIT_SLEEP_STATE::kNormal);
                    actor->NotifyAnimationGraph(RE::BSFixedString("IdleForceDefaultState"));
                } else actor->InitiateGetUpPackage();
                state.retrySeconds=1.0f;
            }
        } else state.furnitureSeconds=0;
        const bool fight=LegionCanFight(autoFight_,peace_,freeze_,pacified_.contains(id),state.waiting);
        auto currentEnemy=actor->GetActorRuntimeData().currentCombatTarget.get();
        const bool friendlyFire=currentEnemy && IsLegionFriendly(currentEnemy.get());
        // Do NOT stop legitimate combat or replace its package with a follow package.
        if(actor->IsInCombat() && (!fight || friendlyFire)) actor->StopCombat();
        if(fight && !furniture && !otherArea && actor->GetActorRuntimeData().currentProcess && actor->Get3D()) {
            ID target=PickEnemy(threats,battleRadius_);
            bool currentValid=false;
            if(currentEnemy && !IsLegionFriendly(currentEnemy.get())) {
                currentValid=std::any_of(threats.begin(),threats.end(),[&](const CombatCandidate& c){return c.id==currentEnemy->GetFormID();});
                if(currentValid && !focusEnemy_) target=currentEnemy->GetFormID();
            }
            if(actor->IsInCombat() && currentEnemy && !currentValid) actor->StopCombat();
            if(target) {
                auto* targetRef=Ref(target); auto* enemy=targetRef?targetRef->As<RE::Actor>():nullptr;
                if(enemy && (!actor->IsInCombat() || !currentEnemy || currentEnemy->GetFormID()!=target) && state.combatCooldown<=0) {
                    if(state.package && actor->GetCurrentPackage()==state.package) actor->EndInterruptPackage(false);
                    state.package=nullptr;
                    const bool accepted=actor->StartCombat(enemy);
                    state.combatCooldown=1.25f;
                    state.status=accepted?"护卫作战："+Label(enemy):"攻击命令被引擎拒绝；将重试";
                }
            }
            if(actor->IsInCombat()) { state.goalProgress.Reset(); state.status="作战中（不覆盖战斗包）"; continue; }
        }
        // Combat can persist across a cell transition. Never teleport or replace
        // a legitimate combat package just because its target is not loaded here.
        if(actor->IsInCombat()) {state.goalProgress.Reset();state.status="作战中（不覆盖战斗包）";continue;}
        const float desired=actor->IsDragon()?std::max(1600.0f,followDistance_):followDistance_;
        auto offset=FormationOffset(memberSlot,player->GetAngle().z,desired,actor->IsDragon()?240.0f:100.0f);
        auto goal=player->GetPosition(); goal.x+=offset[0]; goal.y+=offset[1];
        const float distance=Distance(Pos(actor->GetPosition()),Pos(player->GetPosition()));
        // Native follow stops near the player, not precisely on a formation slot.
        const float tolerance=runtime10::FollowTolerance(actor,followDistance_,
            state.aliasSlot>=0?state.aliasSlot:static_cast<int>(memberSlot));
        const bool nearPlayer=!otherArea && distance<=tolerance;
        const float effectiveLeash=std::max({teleportDistance_,tolerance+500.0f,Distance(Pos(goal),Pos(player->GetPosition()))+300.0f});
        auto decision=state.goalProgress.Update(dt,Pos(actor->GetPosition()),Pos(player->GetPosition()),tolerance,
            !nearPlayer && dt>0 && state.recoveryCooldown14<=0,otherArea,distance>effectiveLeash,false,forcedFollow_?5.0f:stuckDelay10_);
        state.stuckSeconds10=state.goalProgress.Seconds();
        if(decision.recover && !runtime10::CanRecoverNearPlayer(actor,player)) {
            state.recoveryCooldown14=3;state.status="等待适合该生物的跟随落点（水域 / 地面）";
            SetFollowPackage(actor,state);continue;
        }
        if(decision.recover) {
            state.stuckSeconds10=0;
            if(actor->GetActorRuntimeData().currentProcess) actor->StopInteractingQuick(false);
            // Player anchor is known to be loaded. Do not add an unchecked ring
            // offset through a wall, or snap flyers/aquatic actors onto dry navmesh.
            actor->MoveTo(player);
            if(runtime10::CanSnapToGround(actor) && !player->IsSwimming()) actor->MoveToNearestNavmesh();
            state.package=nullptr; state.retrySeconds=0; ++state.recoveries;
            state.recoveryCooldown14=3;
            PrepareLegionActor(actor,state,true);
            state.status="已纠偏到玩家附近；继续验证原生跟随";
            spdlog::info("Legion9 {:08X}: recovery {}, otherArea={}, prior distance={:.0f}",id,state.recoveries,otherArea,distance);
        }
        if(!actor->GetActorRuntimeData().currentProcess || !actor->Get3D()) { state.status="等待角色模型与 AI 过程加载"; continue; }
        if(furniture && !decision.recover) { state.status="退出固定坐姿 / 睡眠"; continue; }
        if(decision.reassert && state.retrySeconds<=0 && !runtime10::IsOwnPackage(actor->GetCurrentPackage())) {
            PrepareLegionActor(actor,state,true); state.retrySeconds=0;
        }
        SetFollowPackage(actor,state);
        if(forcedFollow_ && !state.package) state.status="跟随包未确认；限频重试 + 卡住纠偏";
        else if(state.package && actor->GetCurrentPackage()==state.package) {
            const auto movement=runtime10::Locomotion(actor);
            if(movement.immobile || (movement.known && !movement.walks && !movement.swims && !movement.flies))
                state.status="种族缺少移动能力；限频位置纠偏（不修改共享种族）";
            else if((state.package->GetFormID()&0xFFF)==runtime10::Fallback)
                state.status=nearPlayer?"保底跟随包已读回；距离内待命":"保底跟随包已读回；持续检查接近进度";
            else state.status=nearPlayer?"别名跟随包已读回；距离内待命":"别名跟随包已读回；持续检查接近进度";
        }
        else if(!actor->IsInCombat()) state.status="原 AI 尝试抢回；重申跟随控制";
    }
}
void Engine::ApplyLegionOrder(const Action& action) {
    // 0 follow all; 1 wait all; 2 gather; 3 stand down; 4 explicit focus; 5 dismiss all.
    if(action.count==4) {
        auto* ref=Ref(action.target); auto* actor=ref?ref->As<RE::Actor>():nullptr;
        if(!actor || IsLegionFriendly(actor) || actor->IsDead(false) || actor->IsDisabled()) { Note("集火目标必须是非友方的存活角色。"); return; }
        focusEnemy_=action.target; Note("已设置军团集火目标；和平 / 冻结模式仍优先。"); return;
    }
    std::vector<ID> ids; for(const auto& [id,state]:followers_) ids.push_back(id);
    std::sort(ids.begin(),ids.end());
    auto* player=RE::PlayerCharacter::GetSingleton();
    for(ID id:ids) {
        if(action.count==5) { Dismiss(id); continue; }
        if(action.count==0 || action.count==1) {WaitFollower(id,action.count==1);continue;}
        auto it=followers_.find(id); if(it==followers_.end()) continue;
        auto& state=it->second; auto* r=Ref(id); auto* actor=r?r->As<RE::Actor>():nullptr;
        if(action.count>=0 && action.count<=2) {
            // Orders remain effective for a member whose reference is not currently loaded.
            state.waiting=action.count==1; state.goalProgress.Reset(); state.retrySeconds=0;state.packageAttempts14=0;
        }
        if(!actor || actor->IsDead(false) || actor->IsDisabled()) continue;
        if(action.count==3) { actor->StopCombat(); state.combatCooldown=2; continue; }
        if(action.count==2 && !freeze_ && player && player->GetParentCell() && runtime10::CanRecoverNearPlayer(actor,player) && !(actor->IsDragon() && !dragonsIndoors_ && player->GetParentCell()->IsInteriorCell())) {
            if(actor->GetActorRuntimeData().currentProcess) actor->StopInteractingQuick(false);
            actor->MoveTo(player);
            if(runtime10::CanSnapToGround(actor) && !player->IsSwimming()) actor->MoveToNearestNavmesh();
            state.recoveryCooldown14=3;
        }
        if(state.waiting) { actor->StopCombat(); actor->EnableAI(false); }
        else if(!freeze_) { PrepareLegionActor(actor,state,true); SetFollowPackage(actor,state); }
    }
    if(action.count==3) { focusEnemy_=0; autoFight_=false; Note("全军停战：自动迎敌同时关闭，避免下一轮马上开战。"); }
    else Note("军团命令已应用。");
    dirty_=true;
}

void Engine::DeathAttempt(RE::Actor* actor) {
    if(!actor || actor->GetFormID()==PlayerID || actor->IsDead(false)) return;
    auto* base=actor->GetActorBase(); if(!base) return;
    // Temporary shared-base flags exist only during the synchronous native death call.
    // Restore immediately: never leave all copies of a soldier template unprotected.
    auto& flags=base->actorData.actorBaseFlags;
    const auto old=flags;
    struct Restore {
        decltype(flags)& value; decltype(old) saved;
        ~Restore() {
            for(auto flag:{RE::ACTOR_BASE_DATA::Flag::kEssential,RE::ACTOR_BASE_DATA::Flag::kProtected,
                RE::ACTOR_BASE_DATA::Flag::kInvulnerable,RE::ACTOR_BASE_DATA::Flag::kIsGhost}) {
                if(saved.any(flag)) value.set(flag); else value.reset(flag);
            }
        }
    } restore{flags,old};
    flags.reset(RE::ACTOR_BASE_DATA::Flag::kEssential);
    flags.reset(RE::ACTOR_BASE_DATA::Flag::kProtected);
    flags.reset(RE::ACTOR_BASE_DATA::Flag::kInvulnerable);
    flags.reset(RE::ACTOR_BASE_DATA::Flag::kIsGhost);
    auto& runtime=actor->GetActorRuntimeData();
    runtime.boolFlags.reset(RE::Actor::BOOL_FLAGS::kEssential);
    runtime.boolFlags.reset(RE::Actor::BOOL_FLAGS::kProtected);
    runtime.boolFlags.reset(RE::Actor::BOOL_FLAGS::kIsInKillMove);
    runtime.boolFlags.reset(RE::Actor::BOOL_FLAGS::kScenePackage);
    runtime.boolFlags.reset(RE::Actor::BOOL_FLAGS::kMovementBlocked);
    actor->EnableAI(true); // a previously waiting/disabled-AI actor must be able to finish ragdoll/death
    actor->InterruptCast(false); actor->StopCombat();
    actor->SetCurrentScene(nullptr);
    if(runtime.currentProcess) actor->StopInteractingQuick(false);
    const float health=actor->GetActorValue(RE::ActorValue::kHealth);
    const float damage=Finite(health+1000.0f,1000000,1000,1.0e9f);
    actor->KillImpl(RE::PlayerCharacter::GetSingleton(),damage,true,true);
    // The native event/ragdoll path is preferred; do not fake IsDead by setting a bit.
    if(!actor->IsDead(false)) actor->KillImmediate();
    actor->AddChange(RE::Actor::ChangeFlags::kLifeState);
}
void Engine::ForceDeath(RE::Actor* actor,bool fallback) {
    if(!actor || actor->GetFormID()==PlayerID || actor->IsDeleted()) { Note("请选择非玩家的有效角色引用。"); return; }
    const ID id=actor->GetFormID(); auto* base=actor->GetBaseObject(); if(!base) return;
    Dismiss(id); pacified_.erase(id); frozen_.erase(id);
    if(actor->IsDead(false)) { Note("目标已经死亡；尸体仍存在。要无尸体消失，请选“彻底移除”。"); return; }
    DeathJob job{base->GetFormID(),1,0.3f,fallback};
    DeathAttempt(actor);
    deathJobs_[id]=job; // re-check after simulation advances, including script overrides
    Note("已执行解除保护与原生死亡流程；将在后续帧读回结果。"); dirty_=true;
}
void Engine::RemoveReference(RE::TESObjectREFR* ref) {
    if(!ref || ref->GetFormID()==PlayerID) { Note("不能移除玩家或空引用。"); return; }
    auto* base=ref->GetBaseObject(); if(!base) { Note("目标没有有效基础记录。"); return; }
    const ID id=ref->GetFormID(); const std::string name=Label(ref);
    if(!removed_.contains(id))removalRing10_.push_back(id);
    removed_[id]={base->GetFormID(),name};
    std::erase_if(births10_,[id](const Birth10& birth){return birth.id==id;});
    if(followers_.contains(id))ReleaseFollower10(id);
    enemies10_.erase(id);
    // Never resurrect/evaluate a dying actor by dismissing through its old quest.
    followers_.erase(id); pacified_.erase(id); frozen_.erase(id); deathJobs_.erase(id); spawned_.erase(id);
    if(auto* actor=ref->As<RE::Actor>()) {
        actor->InterruptCast(false); actor->StopCombat(); actor->SetCurrentScene(nullptr);
        if(auto* processes=RE::ProcessLists::GetSingleton()) processes->StopCombatAndAlarmOnActor(actor,true);
        actor->EnableAI(false); actor->SetAlpha(0.0f);
    }
    if(ref->Get3D()) ref->SetCollision(false);
    if(selected_==id) selected_=0;
    ref->Disable();
    const bool disabled=ref->IsDisabled();
    ref->SetDelete(true); // last reference access; actual memory reclamation belongs to Skyrim
    if(!bulkClear10_)Note(fmt::format("已移除 {} [{:08X}]；禁用读回={}，已登记同引用抑制。",name,id,disabled));
    dirty_=hudDirty_=true;
}
void Engine::MaintainRemovals(float dt) {
    std::vector<ID> deaths;
    for(const auto& [id,job]:deathJobs_) deaths.push_back(id);
    for(ID id:deaths) {
        auto it=deathJobs_.find(id); if(it==deathJobs_.end()) continue;
        auto& job=it->second; job.timer-=dt;
        if(job.timer>0 || dt<=0) continue;
        auto* ref=Ref(id); auto* actor=ref?ref->As<RE::Actor>():nullptr;
        if(!actor || !actor->GetBaseObject() || !MatchesRemoval(id,job.base,actor->GetBaseObject()->GetFormID())) { deathJobs_.erase(it); continue; }
        const auto decision=CheckDeath(actor->IsDead(false),job.attempts,job.fallback);
        if(decision==DeathDecision::Confirmed) { Note("已读回目标死亡："+Label(actor)+"。尸体保留；彻底移除是另一项操作。"); deathJobs_.erase(it); }
        else if(decision==DeathDecision::Retry) { ++job.attempts; job.timer=.5f; DeathAttempt(actor); }
        else if(decision==DeathDecision::Remove) { Note("目标持续拒绝死亡，按当前选项改为彻底移除（不伪报死亡成功）。"); RemoveReference(actor); }
        else { Note("死亡读回失败：目标仍被游戏 / 脚本保护。可改用彻底移除。"); deathJobs_.erase(it); }
    }
    if(removed_.empty()) return;
    removalTimer_-=dt;
    if(removalTimer_>0) return;
    removalTimer_=0.25f; // batch suppression scans; do not sort a large set each frame

    if(removalRing10_.empty())for(const auto& [id,entry]:removed_)removalRing10_.push_back(id);
    const std::size_t n=std::min<std::size_t>(64,removalRing10_.size());
    for(std::size_t i=0;i<n;++i) {
        const ID id=removalRing10_.front();removalRing10_.pop_front();
        auto it=removed_.find(id);if(it==removed_.end())continue;
        removalRing10_.push_back(id);
        auto* ref=Ref(id);if(!ref)continue;
        auto* base=ref->GetBaseObject();if(!base || !MatchesRemoval(id,it->second.base,base->GetFormID()))continue;
        if(!ref->IsDisabled() || (!ref->IsDeleted() && ref->Get3D())) {
            if(auto* actor=ref->As<RE::Actor>()){actor->StopCombat();actor->EnableAI(false);actor->SetAlpha(0);}
            if(ref->Get3D())ref->SetCollision(false);
            ref->Disable();ref->SetDelete(true);
        }
    }
}

}
