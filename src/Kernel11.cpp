#include "PCH.h"
#include "Engine.hpp"
#include "Runtime10.hpp"
#include "CrimeHooks11.hpp"
#include <RE/B/BGSScene.h>
#include <RE/M/Main.h>

namespace fc {
namespace {
bool SameSpace11(RE::TESObjectREFR* a,RE::TESObjectREFR* b) {
    auto* ca=a?a->GetParentCell():nullptr;auto* cb=b?b->GetParentCell():nullptr;
    return ca&&cb&&(ca==cb || (ca->IsExteriorCell()&&cb->IsExteriorCell()&&a->GetWorldspace()==b->GetWorldspace()));
}
float Range11(RE::TESObjectREFR* a,RE::TESObjectREFR* b) {
    auto p=a->GetPosition(),q=b->GetPosition();return Distance({p.x,p.y,p.z},{q.x,q.y,q.z});
}
bool Interactive11(RE::TESObjectREFR* r) {
    if(!r||r->IsDeleted()||r->IsDisabled()||r->As<RE::Actor>()||!r->GetBaseObject())return false;
    auto* b=r->GetBaseObject();
    return b->As<RE::TESObjectDOOR>()||b->As<RE::TESObjectCONT>()||b->As<RE::TESFurniture>()||b->As<RE::TESObjectACTI>()||b->IsInventoryObject();
}
}
void Engine::ProtectObject11(RE::TESObjectREFR* ref,bool unlock) {
    if(!ref||ref->As<RE::Actor>()||!ref->GetBaseObject())return;
    const auto id=ref->GetFormID();
    if(!ownRules11_.contains(id)) {
        if(ownRules11_.size()>=50000)return;
        ownRing11_.push_back(id);
    }
    ownRules11_[id]={ref->GetBaseObject()->GetFormID(),unlock};
}
void Engine::ClearCrime11(bool allFactions) {
    auto* p=RE::PlayerCharacter::GetSingleton();if(!p)return;
    if(allFactions)if(auto* d=RE::TESDataHandler::GetSingleton())for(auto* f:d->GetFormArray<RE::TESFaction>())if(f) {
        if(p->GetCrimeGoldValue(f)!=0) {p->ClearAllCrimeGold(f);f->SetCrimeGold(0);f->SetCrimeGoldViolent(0);}
    }
    p->ClearArrested();p->StopAlarmOnActor();
    p->GetActorRuntimeData().boolFlags.reset(RE::Actor::BOOL_FLAGS::kIsTrespassing);
    auto& flags=p->GetPlayerFlags();flags.goToJailQueued=false;flags.servingJailTime=false;
}
void Engine::TickKernel11(float dt) {
    auto* p=RE::PlayerCharacter::GetSingleton();
    if(!ready_||!p||!p->GetParentCell()) {playerIntent14_.Reset();law11::SetPolicy(false,false);return;}
    law11::SetPolicy(rules11_.noBounty,rules11_.noArrest);
    auto* ui=RE::UI::GetSingleton();
    const bool nativePause=ui&&ui->GameIsPaused();
    const bool dialogue=ui&&ui->IsMenuOpen(RE::BSFixedString("Dialogue Menu"));
    if(p->GetPlayerFlags().isLoading || p->IsDead(false)) {playerIntent14_.Reset();return;}
    if(!rules11_.playerFreedom14)playerIntent14_.Reset();
    if(nativePause) {playerIntent14_.Pause(dialogue);return;}
    if(law11::TakeClearRequest())ClearCrime11(true);
    if(resting11_) {
        playerIntent14_.Reset();
        auto& info=p->GetInfoRuntimeData();
        const auto before=info.sleepSeconds;
        for(int i=0;i<8&&info.sleepSeconds;++i)p->AdvanceSleepWaitTick();
        if(!info.sleepSeconds) {resting11_=false;Note("自由等待完成。");}
        else if(info.sleepSeconds==before) {resting11_=false;Note("等待计时未推进，已停止本工具的推进循环。可用释放控制结束受限状态。");}
        return;
    }
    const bool ownMenu=menuOpen_.load()||inputBlocked_||pauseLease_.Held();
    const bool suspended=std::chrono::steady_clock::now()<suspendUntil11_;
    const bool nativeMenu=ui&&!dialogue&&(ui->IsItemMenuOpen()||ui->IsModalMenuOpen()||ui->IsApplicationMenuOpen());
    bool protectedDialogue=dialogue;
    if(!ownMenu&&!nativeMenu&&!suspended)protectedDialogue=TickPlayerFreedom14(dialogue);
    else if(suspended)playerIntent14_.Reset();
    else playerIntent14_.Pause(dialogue);
    if(!ownMenu&&!nativeMenu&&!suspended) {
        if(auto* controls=RE::ControlMap::GetSingleton()) {
            std::uint32_t enabled{},stored{};controls->GetControlsState(enabled,stored);
            auto controlsRule=rules11_;
            if(rules11_.playerFreedom14) {controlsRule.controlGuard=true;controlsRule.controlMask=kKnownControls11;}
            const auto desired=RestoreControls11(enabled,controlsRule,false,protectedDialogue,false);
            if(desired!=enabled) {controls->SetControlsState(desired,stored);++controlsRestored11_;}
        }
        if((rules11_.noAIDriven||rules11_.playerFreedom14)&&!protectedDialogue) {
            const auto& f=p->GetPlayerFlags();
            if(f.aiControlledPackage||f.aiControlledFromPos||f.aiControlledToPos) {p->SetAIDriven(false);++controlsRestored11_;}
        }
        if((rules11_.controlGuard||rules11_.playerFreedom14)&&!protectedDialogue) {
            auto& d=p->GetActorRuntimeData();
            if(rules11_.playerFreedom14||(rules11_.controlMask&1u))d.boolFlags.reset(RE::Actor::BOOL_FLAGS::kMovementBlocked);
            if(rules11_.playerFreedom14||(rules11_.controlMask&(1u<<6))) {d.boolFlags.reset(RE::Actor::BOOL_FLAGS::kAttackingDisabled);d.boolFlags.reset(RE::Actor::BOOL_FLAGS::kCastingDisabled);}
            if(rules11_.playerFreedom14||(rules11_.controlMask&(1u<<1)))d.boolBits.reset(RE::Actor::BOOL_BITS::kHeadingFixed);
            if(rules11_.playerFreedom14 && p->GetLifeState()==RE::ACTOR_LIFE_STATE::kRestrained)p->SetLifeState(RE::ACTOR_LIFE_STATE::kAlive);
        }
        sceneTimer11_+=dt;
        if(sceneTimer11_>=0.4f) {
            sceneTimer11_=0;
            if(rules11_.breakScenes&&!protectedDialogue&&p->GetCurrentScene()) {
                p->StopCurrentDialogue();p->SetCurrentScene(nullptr);
                p->GetActorRuntimeData().boolFlags.reset(RE::Actor::BOOL_FLAGS::kScenePackage);
                if(p->GetActorRuntimeData().currentProcess)p->EndInterruptPackage(false);
                ++controlsRestored11_;
            }
            if(rules11_.breakFurniture&&p->GetOccupiedFurniture().get()&&p->GetActorRuntimeData().currentProcess) {p->StopInteractingQuick(false);++controlsRestored11_;}
        }
        if(rules11_.aimedOwnership&&!dialogue)if(auto* pick=RE::CrosshairPickData::GetSingleton()) {
            auto r=pick->GetActiveTarget().get();
            if(r&&Interactive11(r.get())&&SameSpace11(r.get(),p)&&Range11(r.get(),p)<=1024) {
                const bool needs=r->IsActivationBlocked()||r->IsLocked()||!r->IsAnOwner(p,true,false);
                if(needs) {Own(r.get());++activationRepairs11_;}
            }
        }
    }
    if(rules11_.noSaveWaitLock) {
        auto& flags=p->GetGameStatsData().byCharGenFlag;
        flags.reset(RE::PlayerCharacter::ByCharGenFlag::kDisableSaving);
        flags.reset(RE::PlayerCharacter::ByCharGenFlag::kDisableWaiting);
    }
}
void Engine::MaintainKernel11(float dt,const std::vector<ID>& actors) {
    auto* p=RE::PlayerCharacter::GetSingleton();if(!p)return;
    kernelTimer11_+=dt;
    if(rules11_.noArrest) {p->ClearArrested();p->GetPlayerFlags().goToJailQueued=false;p->GetPlayerFlags().servingJailTime=false;}
    if(rules11_.guardTruce)for(ID id:actors) {
        auto* ref=Ref(id);auto* a=ref?ref->As<RE::Actor>():nullptr;
        if(!a||a==p||enemies10_.contains(id)||!GuardCandidate11(a->IsGuard(),a->IsDead(false),IsLegionFriendly(a),SameSpace11(a,p),Range11(a,p),rules11_.guardRadius))continue;
        auto& rd=a->GetActorRuntimeData();auto target=rd.currentCombatTarget.get();
        const bool crime=rd.boolFlags.any(RE::Actor::BOOL_FLAGS::kCrimeSearch)||rd.boolFlags.any(RE::Actor::BOOL_FLAGS::kAngryWithPlayer);
        const bool greeting=rd.boolBits.any(RE::Actor::BOOL_BITS::kForceGreetingPlayer);
        const bool hostileToUs=target && (target.get()==p||IsLegionFriendly(target.get()));
        if(!crime&&!hostileToUs)continue;
        a->StopAlarmOnActor();if(hostileToUs)a->StopCombat();
        rd.boolFlags.reset(RE::Actor::BOOL_FLAGS::kAngryWithPlayer);rd.boolFlags.reset(RE::Actor::BOOL_FLAGS::kCrimeSearch);
        rd.boolBits.reset(RE::Actor::BOOL_BITS::kAttackOnNextTheft);
        if(greeting) {a->StopCurrentDialogue();a->EndInterruptPackage(false);rd.boolBits.reset(RE::Actor::BOOL_BITS::kForceGreetingPlayer);}
        ++guardReleases11_;dirty_=true;
    }
    if(rules11_.protectClaims) {
        for(int i=0,n=static_cast<int>(std::min<std::size_t>(ownRing11_.size(),32));i<n;++i) {
            ID id=ownRing11_.front();ownRing11_.pop_front();auto it=ownRules11_.find(id);if(it==ownRules11_.end())continue;
            ownRing11_.push_back(id);auto* r=Ref(id);
            if(!r||r->IsDeleted()||r->IsDisabled()||r->As<RE::Actor>()||!r->GetBaseObject()||r->GetBaseObject()->GetFormID()!=it->second.base)continue;
            auto* cell=r->GetParentCell();if(!cell||!cell->IsAttached())continue;
            bool repaired=false;
            if(r->IsActivationBlocked()){r->SetActivationBlocked(false);repaired=true;}
            if(r->GetOwner()!=p->GetActorBase()){r->SetOwner(p->GetActorBase());r->AddChange(RE::TESObjectREFR::ChangeFlags::kOwnershipExtra);repaired=true;}
            if(it->second.unlock&&r->IsLocked()){Run("unlock",r);repaired=true;}
            if(repaired){++activationRepairs11_;dirty_=hudDirty_=true;}
        }
    }
    if(kernelTimer11_>=1.0f) {
        kernelTimer11_=0;
        if(rules11_.noBounty)ClearCrime11(true);
        if(rules11_.unlimitedTraining)p->GetInfoRuntimeData().skillTrainingsThisLevel=0;
        if(rules11_.keepFastTravel)Run("enablefasttravel 1");
        if(rules11_.protectClaims)if(auto* cell=p->GetParentCell())if(auto it=claims_.find(cell->GetFormID());it!=claims_.end()) {
            auto* owner=cell->GetOwner();const auto* name=cell->GetName();
            if(owner!=p->GetActorBase()||!name||it->second.name!=name)ConfigureClaim(it->second);
        }
    }
}
void Engine::ApplyKernel11(const Action& a) {
    auto* p=RE::PlayerCharacter::GetSingleton();if(!p)return;
    switch(a.op) {
    case Op::KernelRules11:rules11_=a.rules11;rules11_.Normalize();if(!rules11_.playerFreedom14||!rules11_.preserveVoluntary14)playerIntent14_.Reset();Note("自由内核规则已应用。每项独立控制，修改只属于当前存档。");break;
    case Op::KernelPreset11:
        rules11_=KernelRules11::Preset(std::clamp(a.count,0,2));suspendUntil11_={};playerIntent14_.Reset();
        Note(a.count==0?"持续守护已关闭，不撤销已完成的接管、任务阶段或删除。":(a.count==1?"自由预设开启：守卫豁免、控制守护、准星解封、旅行与存档等待标志。":"强自由预设开启：另解除玩家场景和家具占用、训练次数限制。想正常坐椅子时关闭家具脱离。"));break;
    case Op::KernelSuspend11:playerIntent14_.Reset();suspendUntil11_=std::chrono::steady_clock::now()+std::chrono::seconds(std::clamp(a.count,0,300));Note("临时允许剧情控制（不关闭赏金/监禁豁免），期满自动恢复。");break;
    case Op::ClearCrime11:ClearCrime11(true);Note("已清理当前赏金与被捕状态。");break;
    case Op::ProtectObject11:
        if(a.count)ProtectObject11(Ref(a.target),true);else ownRules11_.erase(a.target);
        Note(a.count?"已加入单对象接管守护。":"已停止守护该对象；不会主动还原所有权。");break;
    case Op::OwnUse11: {
        auto* r=Ref(a.target);if(!Interactive11(r)){Note("请选择有效的门、容器、家具或可激活对象。");break;}
        Own(r);
        if(a.scope==2) {DoorTransit10(r);break;}
        const bool ok=r->ActivateRef(p,0,nullptr,1,a.scope==1);
        Note(ok?"已接管、解锁、解除激活封锁并请求使用。":"引擎拒绝激活；有配对门时可点直接进入。脚本谜题不会被伪造为已完成。");break;
    }
    case Op::Rest11: {
        if(p->IsDead(false)||p->GetInfoRuntimeData().sleepSeconds) {Note("玩家已死亡或已有睡眠/等待，未重复启动。");break;}
        p->StartWaiting(std::clamp(a.count,1,72));resting11_=p->GetInfoRuntimeData().sleepSeconds!=0;
        Note(resting11_?"已启动自由等待；由原生时钟分批推进，不检查附近敌人或床归属。":"引擎未进入等待状态；没有伪报时间已经推进。");break;
    }
    default:break;
    }
    law11::SetPolicy(ready_&&rules11_.noBounty,ready_&&rules11_.noArrest);
    if(rules11_.noBounty||rules11_.noArrest)ClearCrime11(rules11_.noBounty);
    dirty_=hudDirty_=true;
}
void Engine::ResetKernel11() {
    playerIntent14_.Reset();freedomActionAt14_={};rejectedDialogues14_=releasedScenes14_=0;
    law11::SetPolicy(false,false);rules11_=KernelRules11::Preset(1);lastPersonalSlot11_=-1;lastPersonalRequest11_=0;suspendUntil11_={};
    ownRules11_.clear();ownRing11_.clear();blockedQuests11_.clear();questRows11_.reset();questDetail11_={};questSelected11_=0;
    for(auto& q:personal11_)q.reset();
    syncedPersonal11_.fill(0);
    originalQuestText11_.clear();questEdits11_.clear();kernelTimer11_=questTimer11_=sceneTimer11_=0;
    controlsRestored11_=guardReleases11_=activationRepairs11_=questStops11_=0;resting11_=false;questDirty11_=true;
}
}
