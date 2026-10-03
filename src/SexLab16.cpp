#include "PCH.h"
#include "Engine.hpp"
#include "fc/VMArguments18.hpp"
#include <RE/V/VirtualMachine.h>

namespace fc {
namespace {
constexpr RE::FormID kSexLabFrameworkLocal16 = 0x0D62;
constexpr std::string_view kSexLabPlugin16 = "SexLab.esm";
constexpr std::string_view kSexLabScript16 = "SexLabFramework";
constexpr std::size_t kSexLabMaxActors16 = 5;

RE::TESQuest* SexLabQuest16() {
    auto* data=RE::TESDataHandler::GetSingleton();
    return data ? data->LookupForm<RE::TESQuest>(kSexLabFrameworkLocal16,kSexLabPlugin16) : nullptr;
}

bool BoundScript16(RE::TESQuest* quest, std::string_view script, RE::BSTSmartPointer<RE::BSScript::Object>& object) {
    if (!quest) return false;
    auto* vm=RE::BSScript::Internal::VirtualMachine::GetSingleton();
    auto* policy=vm ? vm->GetObjectHandlePolicy() : nullptr;
    if (!vm || !policy) return false;
    const auto handle=policy->GetHandleForObject(static_cast<RE::VMTypeID>(quest->GetFormType()),quest);
    if (!handle) return false;
    return vm->FindBoundObject(handle,script.data(),object) && object;
}
bool BoundFramework16(RE::TESQuest* quest, RE::BSTSmartPointer<RE::BSScript::Object>& object) {
    return BoundScript16(quest,kSexLabScript16,object);
}

std::string ActorLabel16(RE::Actor* actor, ID id) {
    std::string name;
    if (actor) if (const char* raw=actor->GetName();raw && *raw) name=raw;
    if (name.empty()) name="Actor";
    return fmt::format("{} [{}]",name,Hex(id));
}
}

void Engine::RefreshSexLab16(View& next) {
    auto* quest=SexLabQuest16();
    next.sexLabInstalled16=quest!=nullptr;
    RE::BSTSmartPointer<RE::BSScript::Object> object;
    next.sexLabBound16=BoundFramework16(quest,object);
    next.sexLabStatus16=sexLabStatus16_;
    next.audioAvailable16=audioAvailable16_;
    next.audioMuted16=audioMuted16_;
    next.sexLabActors16.clear();
    next.sexLabActors16.reserve(sexLabActors16_.size());
    for (const auto id:sexLabActors16_) {
        auto* ref=Ref(id); auto* actor=ref?ref->As<RE::Actor>():nullptr;
        next.sexLabActors16.push_back({id,ActorLabel16(actor,id),false,0.0f,
            actor ? (actor->IsDead(false)?"已死亡":"可用") : "当前不可用"});
    }
}

void Engine::ApplySexLab16(const Action& a) {
    auto actorFor=[&](ID id)->RE::Actor* {
        auto* ref=Ref(id);return ref?ref->As<RE::Actor>():nullptr;
    };
    switch(a.op) {
    case Op::SexLabAdd16: {
        const ID id=a.target;
        auto* actor=actorFor(id);
        if (!actor) { sexLabStatus16_="只能把当前有效 Actor 加入参与者。"; Note(sexLabStatus16_); break; }
        if (actor->IsDead(false)) { sexLabStatus16_="目标已死亡，未加入 SexLab 参与者。"; Note(sexLabStatus16_); break; }
        if (std::find(sexLabActors16_.begin(),sexLabActors16_.end(),id)!=sexLabActors16_.end()) {
            sexLabStatus16_="该角色已经在参与者列表中。"; break;
        }
        if (sexLabActors16_.size()>=kSexLabMaxActors16) {
            sexLabStatus16_="SexLab QuickStart 最多接收 5 名参与者。"; Note(sexLabStatus16_); break;
        }
        sexLabActors16_.push_back(id);
        sexLabStatus16_=fmt::format("已加入 {}，当前 {} / 5。",ActorLabel16(actor,id),sexLabActors16_.size());
        break;
    }
    case Op::SexLabRemove16:
        std::erase(sexLabActors16_,a.target);
        sexLabStatus16_=fmt::format("参与者剩余 {} / 5。",sexLabActors16_.size());
        break;
    case Op::SexLabMove16: {
        auto it=std::find(sexLabActors16_.begin(),sexLabActors16_.end(),a.target);
        if (it==sexLabActors16_.end()) break;
        const auto index=static_cast<std::ptrdiff_t>(std::distance(sexLabActors16_.begin(),it));
        const auto next=std::clamp<std::ptrdiff_t>(index+(a.count<0?-1:1),0,static_cast<std::ptrdiff_t>(sexLabActors16_.size()-1));
        if (next!=index) std::iter_swap(sexLabActors16_.begin()+index,sexLabActors16_.begin()+next);
        break;
    }
    case Op::SexLabClear16:
        sexLabActors16_.clear(); sexLabStatus16_="参与者列表已清空。"; break;
    case Op::SexLabStart16:
        DispatchSexLab16(a); break;
    case Op::SexLabStopAll16:
        DispatchSexLabStopAll16(); break;
    default: break;
    }
    dirty_=true;
}

bool Engine::DispatchSexLab16(const Action& a) {
    auto* player=RE::PlayerCharacter::GetSingleton();
    if (!player) { sexLabStatus16_="玩家引用不可用。"; Note(sexLabStatus16_); return false; }

    // Use the already validated runtime player, not another TU's local constant.
    const ID playerID = player->GetFormID();
    std::vector<ID> requested;
    if (a.scope==1) {
        requested.push_back(playerID);
        if (a.target && a.target!=playerID) requested.push_back(a.target);
    } else requested=sexLabActors16_;

    std::array<RE::Actor*,kSexLabMaxActors16> actors{};
    std::size_t count{};
    std::unordered_set<ID> seen;
    for (const auto id:requested) {
        if (!id || !seen.insert(id).second || count>=actors.size()) continue;
        auto* ref=Ref(id); auto* actor=ref?ref->As<RE::Actor>():nullptr;
        if (!actor || actor->IsDead(false) || actor->IsDisabled()) continue;
        actors[count++]=actor;
    }
    if (!count) { sexLabStatus16_="没有可用于启动的有效参与者。"; Note(sexLabStatus16_); return false; }

    auto* quest=SexLabQuest16();
    if (!quest) { sexLabStatus16_="未检测到 SexLab.esm 的 Framework Quest。"; Note(sexLabStatus16_); return false; }
    auto* vm=RE::BSScript::Internal::VirtualMachine::GetSingleton();
    auto* policy=vm ? vm->GetObjectHandlePolicy() : nullptr;
    if (!vm || !policy) { sexLabStatus16_="Papyrus VM 当前不可用。"; Note(sexLabStatus16_); return false; }
    const auto handle=policy->GetHandleForObject(static_cast<RE::VMTypeID>(quest->GetFormType()),quest);
    RE::BSTSmartPointer<RE::BSScript::Object> scriptObject;
    if (!handle || !vm->FindBoundObject(handle,kSexLabScript16.data(),scriptObject) || !scriptObject) {
        sexLabStatus16_="SexLabFramework 脚本未绑定；请先确认 SexLab 已在 MCM 中完成初始化。";
        Note(sexLabStatus16_); return false;
    }

    std::string tags=a.text.substr(0,256);
    auto* args=fc::MakeVMArguments18(
        actors[0],actors[1],actors[2],actors[3],actors[4],
        static_cast<RE::Actor*>(nullptr),
        RE::BSFixedString("FreedomControl"),RE::BSFixedString(tags.c_str()));
    RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
    const bool dispatched=vm->DispatchMethodCall(scriptObject,RE::BSFixedString("QuickStart"),args,callback);
    if (dispatched) {
        sexLabStatus16_=fmt::format("已向 SexLab QuickStart 提交 {} 名参与者{}。",count,
            tags.empty()?"（自动匹配动画）":fmt::format("（标签：{}）",tags));
        Note(sexLabStatus16_+" 若没有开始，请检查 SexLab/SLAL 注册状态与角色种族兼容。 ");
        spdlog::info("SexFast16: QuickStart dispatched actors={} tags='{}'",count,tags);
    } else {
        sexLabStatus16_="Papyrus VM 拒绝了 QuickStart 调用；未宣称动画已启动。";
        Note(sexLabStatus16_); spdlog::warn("SexFast16: DispatchMethodCall returned false");
    }
    return dispatched;
}

bool Engine::DispatchSexLabStopAll16() {
    auto* quest=SexLabQuest16();
    if (!quest) { sexLabStatus16_="未检测到 SexLab.esm 的 Framework Quest。"; Note(sexLabStatus16_); return false; }
    auto* vm=RE::BSScript::Internal::VirtualMachine::GetSingleton();
    RE::BSTSmartPointer<RE::BSScript::Object> slotsObject;
    if (!vm || !BoundScript16(quest,"sslThreadSlots",slotsObject) || !slotsObject) {
        sexLabStatus16_="SexLab sslThreadSlots 未绑定；请先完成 SexLab 初始化。";
        Note(sexLabStatus16_); return false;
    }
    auto* args=fc::MakeVMArguments18();
    RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
    const bool dispatched=vm->DispatchMethodCall(slotsObject,RE::BSFixedString("StopAll"),args,callback);
    if (dispatched) {
        sexLabStatus16_="已提交 SexLab StopAll：正在停止并清理全部活动线程。";
        Note(sexLabStatus16_); spdlog::info("SexFast16: sslThreadSlots.StopAll dispatched");
    } else {
        sexLabStatus16_="Papyrus VM 拒绝了 SexLab StopAll 调用。";
        Note(sexLabStatus16_); spdlog::warn("SexFast16: StopAll DispatchMethodCall returned false");
    }
    return dispatched;
}

}
