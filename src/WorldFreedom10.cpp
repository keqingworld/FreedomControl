#include "PCH.h"
#include "Engine.hpp"
#include "Runtime10.hpp"
#include <RE/E/ExtraMapMarker.h>
#include <RE/E/ExtraTeleport.h>
#include <RE/T/TESSpellCastEvent.h>
#include <RE/T/TESObjectLoadedEvent.h>
#include <RE/S/ScriptEventSourceHolder.h>

namespace fc::runtime10 {
class Events10 final : public RE::BSTEventSink<RE::TESSpellCastEvent>,public RE::BSTEventSink<RE::TESObjectLoadedEvent> {
    RE::BSEventNotifyControl ProcessEvent(const RE::TESSpellCastEvent* event,RE::BSTEventSource<RE::TESSpellCastEvent>*) override {
        if(event && event->object)Engine::Get().OnSpellCast10(event->object->GetFormID(),event->spell);
        return RE::BSEventNotifyControl::kContinue;
    }
    RE::BSEventNotifyControl ProcessEvent(const RE::TESObjectLoadedEvent* event,RE::BSTEventSource<RE::TESObjectLoadedEvent>*) override {
        if(event && event->loaded)Engine::Get().OnLoaded10(event->formID);
        return RE::BSEventNotifyControl::kContinue;
    }
};
void InstallEvents(){static Events10 events;static bool installed=false;if(!installed)if(auto* src=RE::ScriptEventSourceHolder::GetSingleton()){src->AddEventSink<RE::TESSpellCastEvent>(&events);src->AddEventSink<RE::TESObjectLoadedEvent>(&events);installed=true;}}
}
namespace fc {
namespace {
std::vector<ID> ReferenceSnapshot10(bool actors) {
    std::vector<ID> ids;
    const auto [map,lock]=RE::TESForm::GetAllForms();
    const RE::BSReadLockGuard guard(lock);
    if(map)for(const auto& [id,form]:*map)if(form && id!=0x14 && !form->IsDeleted()) {
        if(actors){if(form->As<RE::Actor>())ids.push_back(id);}
        else if(form->AsReference())ids.push_back(id);
    }
    return ids; // engine mutations must occur AFTER releasing the map read lock
}
bool SameRegion10(RE::TESObjectREFR* ref,RE::TESObjectREFR* player) {
    auto* a=ref?ref->GetParentCell():nullptr;auto* b=player?player->GetParentCell():nullptr;
    return a && b && (a==b || (a->IsExteriorCell() && b->IsExteriorCell() && ref->GetWorldspace()==player->GetWorldspace()));
}
float Separation10(RE::TESObjectREFR* a,RE::TESObjectREFR* b){auto p=a->GetPosition(),q=b->GetPosition();return Distance({p.x,p.y,p.z},{q.x,q.y,q.z});}
}
void Engine::OnSpellCast10(ID caster,ID spell) {
    // Event callback only queues intent; it never deletes actors during the event dispatch.
    if(caster!=0x14)return;
    auto* power=runtime10::Form<RE::SpellItem>(runtime10::Spell);
    if(power && power->GetFormID()==spell)Submit(Action{.op=Op::CastPower10});
}
void Engine::OnLoaded10(ID id) { if(id && id!=0x14)Submit(Action{.op=Op::LoadedActor10,.target=id}); }
void Engine::QueueClear10(bool world) {
    auto* player=RE::PlayerCharacter::GetSingleton();if(!player || !player->GetParentCell())return;
    const auto snapshot=ReferenceSnapshot10(true);
    if(clearJobs10_.empty())clearDone10_=clearTotal10_=0;
    for(ID id:snapshot) {
        auto* r=Ref(id);auto* actor=r?r->As<RE::Actor>():nullptr;if(!actor)continue;
        auto* base=actor->GetBaseObject();
        const ClearFacts10 facts{id,base?base->GetFormID():0,Separation10(actor,player),true,SameRegion10(actor,player),IsLegionFriendly(actor),actor->IsDeleted()};
        if(ClearEligible10(facts,clearRadius10_,world,protectArmy10_) && !removed_.contains(id) && clearQueued10_.insert(id).second) {
            clearJobs10_.push_back({id,facts.base,epoch_});++clearTotal10_;
        }
    }
    Note(fmt::format("keqing 清理：{}，排队 {} 个现有角色引用；每帧分批处理，不删除角色模板。",world?"全局已实例化引用":"当前位置半径",clearJobs10_.size()));
    dirty_=true;
}
void Engine::ProcessClear10() {
    if(clearJobs10_.empty())return;
    const auto start=std::chrono::steady_clock::now();
    struct Flag {bool& ref;explicit Flag(bool& x):ref(x){ref=true;}~Flag(){ref=false;}} quiet(bulkClear10_);
    for(int n=0;n<64 && !clearJobs10_.empty();++n) {
        const auto job=clearJobs10_.front();clearJobs10_.pop_front();clearQueued10_.erase(job.id);
        if(job.epoch!=epoch_)continue;
        auto* r=Ref(job.id);auto* actor=r?r->As<RE::Actor>():nullptr;
        if(actor && !actor->IsDeleted() && actor->GetFormID()!=0x14 && actor->GetBaseObject() && actor->GetBaseObject()->GetFormID()==job.base && !(protectArmy10_ && IsLegionFriendly(actor)))RemoveReference(actor);
        ++clearDone10_;dirty_=true;
        if(std::chrono::steady_clock::now()-start>std::chrono::milliseconds(4))break;
    }
    if(clearJobs10_.empty())Note(fmt::format("keqing 清理队列完成：已处理 {} 条（已消失 / 新增保护目标跳过）。",clearDone10_));
}
void Engine::MaintainFreedom10(float dt,const std::vector<ID>& actors) {
    EnsureRuntime10();
    // Process newly loaded actors as they become available, not fictional unloaded entities.
    if(aura10_ || emptyWorld10_) {
        auto* player=RE::PlayerCharacter::GetSingleton();if(!player)return;
        for(ID id:actors) {
            auto* r=Ref(id);auto* actor=r?r->As<RE::Actor>():nullptr;if(!actor)continue;
            auto* base=actor->GetBaseObject();
            ClearFacts10 facts{id,base?base->GetFormID():0,Separation10(actor,player),true,SameRegion10(actor,player),IsLegionFriendly(actor),actor->IsDeleted()};
            if(ClearEligible10(facts,clearRadius10_,emptyWorld10_,protectArmy10_) && !removed_.contains(id) && clearQueued10_.insert(id).second){clearJobs10_.push_back({id,facts.base,epoch_});++clearTotal10_;}
        }
    }
    freedomTimer10_+=dt;
    if(freedomTimer10_>=1.5f){
        freedomTimer10_=0;
        if(keepTravel10_)Run("enablefasttravel 1");
        if(!peace_ && !freeze_)for(ID id:enemies10_) {
            auto* r=Ref(id);auto* a=r?r->As<RE::Actor>():nullptr;auto* p=RE::PlayerCharacter::GetSingleton();
            if(a && p && !a->IsDead(false) && !a->IsDisabled() && !a->IsInCombat() && a->Get3D() && a->GetActorRuntimeData().currentProcess && SameRegion10(a,p) && Separation10(a,p)<=battleRadius_ && !followers_.contains(id))a->StartCombat(p);
        }
    }
}
void Engine::DoorTransit10(RE::TESObjectREFR* door) {
    auto* player=RE::PlayerCharacter::GetSingleton();
    auto* extra=door?door->extraList.GetByType<RE::ExtraTeleport>():nullptr;
    auto linked=extra && extra->teleportData?extra->teleportData->linkedDoor.get():RE::NiPointer<RE::TESObjectREFR>{};
    if(!player || !linked || !linked->GetParentCell()){Note("该对象没有有效的配对传送门。无 XTEL 的脚本门 / 实体闸门不适用。");return;}
    const auto position=extra->teleportData->position,rotation=extra->teleportData->rotation;
    if(!IsFinitePoint({position.x,position.y,position.z})){Note("门目标坐标无效。");return;}
    lastMarker_=MarkerAtPlayer(lastMarker_);
    // Same primitive as CommonLib's MoveTo_Impl (private C++ wrapper). The known
    // AE relocation is resolved via Address Library, never a raw runtime address.
    using MoveFn=void(*)(RE::TESObjectREFR*,const RE::ObjectRefHandle&,RE::TESObjectCELL*,RE::TESWorldSpace*,const RE::NiPoint3&,const RE::NiPoint3&);
    static REL::Relocation<MoveFn> move{REL::RelocationID(56227,56626)};
    const auto handle=linked->GetHandle();
    move(player,handle,linked->GetParentCell(),linked->GetWorldspace(),position,rotation);
    Note("已按传送门目标位置直接移动；未经过原版开门 / 快速旅行条件判断。");
}
void Engine::RefreshTravel10() {
    travel10_.clear();
    for(ID id:ReferenceSnapshot10(false))if(auto* r=Ref(id)) {
        if(r->extraList.GetByType<RE::ExtraMapMarker>()) {
            const auto* name=r->GetDisplayFullName();
            travel10_.push_back({id,name&&*name?name:"["+Hex(id)+"]",false,0,{}});
        }
    }
    std::sort(travel10_.begin(),travel10_.end(),[](const NamedRef& a,const NamedRef& b){return a.name!=b.name?a.name<b.name:a.id<b.id;});
    Note(fmt::format("自由旅行索引 {} 个可解析地图标记。可直接移动，不经过原版旅行许可。",travel10_.size()));dirty_=true;
}
void Engine::ApplyFreedom10(const Action& a) {
    auto* player=RE::PlayerCharacter::GetSingleton();
    switch(a.op) {
    case Op::SpawnRandom10:QueueRandom10(a,false);break;
    case Op::SpawnSoldiers10:QueueRandom10(a,true);break;
    case Op::DoorTransit10:DoorTransit10(Ref(a.target));break;
    case Op::RefreshTravel10:RefreshTravel10();break;
    case Op::LoadedActor10: {
        auto* ref=Ref(a.target);auto* actor=ref?ref->As<RE::Actor>():nullptr;
        if(!actor || !player || actor==player || !actor->GetBaseObject())break;
        const ID base=actor->GetBaseObject()->GetFormID();
        if(auto it=removed_.find(a.target);it!=removed_.end() && it->second.base==base) {
            // No full-world scan and no duplicate tombstone insertion on a re-enable event.
            actor->StopCombat();actor->EnableAI(false);actor->SetAlpha(0);
            if(actor->Get3D())actor->SetCollision(false);
            actor->Disable();actor->SetDelete(true);break;
        }
        if(aura10_ || emptyWorld10_) {
            ClearFacts10 facts{a.target,base,Separation10(actor,player),true,SameRegion10(actor,player),IsLegionFriendly(actor),actor->IsDeleted()};
            if(ClearEligible10(facts,clearRadius10_,emptyWorld10_,protectArmy10_) && clearQueued10_.insert(a.target).second){clearJobs10_.push_back({a.target,base,epoch_});++clearTotal10_;}
        }
        break;
    }
    case Op::ClearConfig10:
        clearRadius10_=Finite(a.value[0],2000,1,100000);protectArmy10_=a.protectArmy;
        aura10_=a.value[1]!=0;emptyWorld10_=a.value[2]!=0;
        if(emptyWorld10_ || aura10_)QueueClear10(emptyWorld10_);
        else {clearJobs10_.clear();clearQueued10_.clear();}
        Note("领域设置已应用；关闭领域只停止后续清理，不复活已删除角色。");break;
    case Op::CastPower10:QueueClear10(false);break;
    case Op::ClearPulse10:clearRadius10_=Finite(a.value[0],clearRadius10_,1,100000);protectArmy10_=a.protectArmy;QueueClear10(false);break;
    case Op::ClearWorld10:protectArmy10_=a.protectArmy;QueueClear10(true);break;
    case Op::ClearCancel10:clearJobs10_.clear();clearQueued10_.clear();aura10_=emptyWorld10_=false;Note("已停止清理队列及持续领域。");break;
    case Op::LearnPower10:
        if(player)if(auto* spell=runtime10::Form<RE::SpellItem>(runtime10::Spell)) {
            player->AddSpell(spell);Note("已添加小能力 keqing - Clear Zone。装备后按龙吼键释放；半径与军团保护取本面板设置。");break;
        }
        Note("能力记录未加载。请勾选 FreedomControlRuntime.esp。");break;
    case Op::RescuePlayer10:
        if(player) {
            player->InterruptCast(false);player->StopCurrentDialogue();player->SetCurrentScene(nullptr);
            if(player->GetActorRuntimeData().currentProcess)player->StopInteractingQuick(false);
            auto& data=player->GetActorRuntimeData();
            data.boolFlags.reset(RE::Actor::BOOL_FLAGS::kMovementBlocked);
            data.boolFlags.reset(RE::Actor::BOOL_FLAGS::kAttackingDisabled);
            data.boolFlags.reset(RE::Actor::BOOL_FLAGS::kCastingDisabled);
            data.boolBits.reset(RE::Actor::BOOL_BITS::kHeadingFixed);
            data.boolBits.reset(RE::Actor::BOOL_BITS::kParalyzed);
            Run("enableplayercontrols");Run("setplayeraidriven 0");
            player->NotifyAnimationGraph(RE::BSFixedString("IdleForceDefaultState"));
            Note("已请求释放玩家控制 / 坐姿 / 场景占用。未停止任意剧情任务。");
        }break;
    case Op::FreedomSettings10:
        keepTravel10_=a.count!=0;stuckDelay10_=Finite(a.value[0],12,5,120);
        if(keepTravel10_)Run("enablefasttravel 1");
        Note("设置已应用。原版地图的敌人/室内限制请使用自由旅行列表绕过；本开关只解除脚本禁用标记。");break;
    default:break;
    }
    dirty_=hudDirty_=true;
}
}
