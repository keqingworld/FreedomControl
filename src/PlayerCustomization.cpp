#include "Engine.hpp"
namespace fc {
void Engine::ApplyPlayer12(const Action& a) {
    auto* player=RE::PlayerCharacter::GetSingleton();
    if (!player || player->IsDead(false)) { Note("玩家未就绪或已死亡，未修改属性。"); return; }
    if (a.op==Op::PlayerLevel12) {
        if (!custom12::ValidLevel(a.count)) { Note("等级范围为 1—65535。"); return; }
        const auto before=player->GetLevel();
        Run(fmt::format("player.setlevel {}",a.count));
        Note(fmt::format("等级读回：{} → {}（请求 {}）；不自动分配技能、Perk 或升级属性奖励。",before,player->GetLevel(),a.count));
        hudDirty_=dirty_=true; return;
    }
    if (a.count<0 || a.count>2) { Note("无效的资源类型。"); return; }
    const bool maximumEdit=a.op==Op::ResourceMax12;
    if ((maximumEdit && !custom12::ValidMaximum(a.value[0])) ||
        (!maximumEdit && !custom12::ValidPercent(a.value[0]))) {
        Note("上限范围 1—1000000；当前比例范围 0—100%。"); return;
    }
    constexpr RE::ActorValue values[]={RE::ActorValue::kHealth,RE::ActorValue::kMagicka,RE::ActorValue::kStamina};
    constexpr const char* names[]={"生命","法力","耐力"};
    const auto av=values[a.count];const bool health=a.count==0;
    ResourceBoost* boost=a.count==0?&healthBoost_:(a.count==1?&magickaBoost_:nullptr);
    auto invalidate=[&] {
        auto* process=player->GetActorRuntimeData().currentProcess;
        auto* cache=process?process->cachedValues:nullptr;
        if(cache) {
            for(auto& entry:cache->actorValueCache)entry.invalid=true;
            for(auto& entry:cache->maxActorValueCache)entry.invalid=true;
        }
    };
    invalidate();
    const float oldMax=player->GetActorValueMax(av),base=player->GetBaseActorValue(av),current=player->GetActorValue(av);
    if(!std::isfinite(oldMax) || !std::isfinite(base) || !std::isfinite(current) || oldMax<0 ||
        (health && (oldMax<1 || current<=0))) {
        Note("资源读回异常，原有开关和数值保持不变。");return;
    }
    const bool boosted=boost && boost->enabled;
    // Plan first. Do not call legacy SetBoost: it changes base and refills other resources.
    const float unboostedBase=boosted?RestoreBoostedBase(base,boost->original,boost->applied):base;
    const float desiredBase=maximumEdit?custom12::BaseForMaximum(base,oldMax,a.value[0]):unboostedBase;
    const float desiredMax=oldMax+(desiredBase-base);
    if(!std::isfinite(desiredBase) || desiredBase<1 || !std::isfinite(desiredMax) || desiredMax<1) {
        Note("目标会产生过低或负的基础属性，已取消。请提高目标上限，或先移除装备 / 临时加成；原有开关未改变。");return;
    }
    const float fraction=maximumEdit?(a.value[1]!=0?1.0f:custom12::Fraction(current,oldMax)):a.value[0]/100.0f;
    const float desired=custom12::DesiredCurrent(desiredMax,fraction,health);
    const bool baseChange=std::abs(desiredBase-base)>0.001f;
    if(baseChange) {
        // Clear outstanding damage BEFORE decreasing the maximum. Otherwise the
        // intermediate current value can cross zero and trigger a death callback.
        // Zero-magicka/stamina can be raised directly without dividing by zero.
        if(oldMax>0 && current<oldMax) {
            player->RestoreActorValue(av,oldMax-current);invalidate();
            if(player->GetActorValue(av)<oldMax-0.01f) {Note("补回损失值未通过读回，未降低上限或改变开关。");return;}
        }
        // Healing may trigger external effects. Re-read before committing a base change.
        if(player->IsDead(false) || std::abs(player->GetActorValueMax(av)-oldMax)>0.01f ||
            std::abs(player->GetBaseActorValue(av)-base)>0.01f) {Note("期间有其他效果改变属性，本次上限修改已停止。");return;}
        player->SetBaseActorValue(av,desiredBase);invalidate();
        // The old enhancement delta is no longer owned once this write replaces it.
        if(boosted && std::abs(player->GetBaseActorValue(av)-base)>0.001f)boost->enabled=false;
    }
    const float actualMax=player->GetActorValueMax(av);
    if(!std::isfinite(actualMax) || actualMax<1 || std::abs(actualMax-desiredMax)>0.1f || player->IsDead(false)) {
        // Do not reapply old damage to an unknown maximum, and never revive a player.
        Note("上限读回与请求不一致，已停止后续修改；请查看实际数值。");return;
    }
    if(!custom12::SetCurrent(*player,av,RE::ACTOR_VALUE_MODIFIER::kDamage,fraction,health)) {
        Note("当前比例未能应用，原有补满开关未改变。");return;
    }
    invalidate();
    const float actual=player->GetActorValue(av);
    if(!std::isfinite(actual) || std::abs(actual-desired)>0.1f) {
        Note("当前值受到其他效果影响，未关闭原有补满开关，请查看实际数值。");return;
    }
    // Commit conflicting settings only after numeric validation and readback.
    if(boosted)boost->enabled=false;
    if(!maximumEdit || a.value[1]==0)unlimited_=false;
    hudDirty_=dirty_=true;
    Note(fmt::format("{}读回：当前 {:.1f} / 上限 {:.1f}；基础 {:.1f}。",names[a.count],actual,actualMax,player->GetBaseActorValue(av)));
}
}
