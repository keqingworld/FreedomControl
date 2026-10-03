#include "PCH.h"
#include "Engine.hpp"
#include "Runtime10.hpp"
#include <RE/T/TESLevCharacter.h>
#include <fstream>

namespace fc {
namespace {
std::string ActorName10(RE::TESObjectREFR* ref){if(!ref)return "<unloaded>";const auto* p=ref->GetDisplayFullName();return p&&*p?p:"["+Hex(ref->GetFormID())+"]";}
bool Area10(RE::TESObjectREFR* a,RE::TESObjectREFR* b) {
    if(!a || !b || !a->GetParentCell() || !b->GetParentCell())return false;
    return a->GetParentCell()==b->GetParentCell() || (a->GetParentCell()->IsExteriorCell() && b->GetParentCell()->IsExteriorCell() && a->GetWorldspace()==b->GetWorldspace());
}
std::string Lower10(std::string s){for(auto& c:s)c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));return s;}
}
void Engine::AnnotateCatalog10(std::vector<Entry>& entries) {
    manifest10_=resolved10_=dc10_=0;catalogStatus10_.clear();
    struct Tags{bool dc{},scripted{};std::string names;};
    std::unordered_map<ID,Tags> explicitIDs;std::unordered_set<ID> creatureRaces;
    try {
        const std::filesystem::path path=L"Data/SKSE/Plugins/FreedomControl.creatures.json";
        if(!std::filesystem::exists(path))throw std::runtime_error("catalog file missing");
        if(std::filesystem::file_size(path)>8*1024*1024)throw std::runtime_error("catalog exceeds 8 MiB");
        std::ifstream file(path);if(!file)throw std::runtime_error("catalog cannot be opened");
        const auto j=nlohmann::json::parse(file);
        if(j.at("schema").get<int>()!=1)throw std::runtime_error("unknown catalog schema");
        auto* data=RE::TESDataHandler::GetSingleton();
        if(!data)return;
        // Never treat armor / skin / race IDs as a spawnable actor. Only the curated NPC_ keys.
        for(const auto& item:j.at("entries")) {
            ++manifest10_;
            const auto plugin=item.at("plugin").get<std::string>();const auto local=item.at("localID").get<ID>();
            auto* npc=data->LookupForm<RE::TESNPC>(local,plugin);
            if(!npc || npc->IsDeleted())continue;
            auto& tag=explicitIDs[npc->GetFormID()];tag.scripted|=item.value("scripted",false);
            for(const auto& source:item.at("sources")) {
                auto name=source.get<std::string>();
                if(!tag.names.empty())tag.names+=" | ";
                tag.names+=name;
                tag.dc|=Lower10(name)=="demoniccreatures.esp";
            }
        }
        // Race matches are an optional separate filter, never advertised as verified animations.
        if(j.contains("races"))for(const auto& item:j.at("races")) {
            auto* race=data->LookupForm<RE::TESRace>(item.at("localID").get<ID>(),item.at("plugin").get<std::string>());
            if(race)creatureRaces.insert(race->GetFormID());
        }
        resolved10_=explicitIDs.size();
    } catch(const std::exception& e) {catalogStatus10_=std::string("专用清单读取失败：")+e.what();spdlog::error("Freedom10 catalog: {}",e.what());}
    for(auto& entry:entries)if(entry.kind==Kind::NPC) {
        const auto editor=Lower10(entry.editor);
        entry.soldier=!entry.unique && !entry.preset && (editor.starts_with("encguard") || editor.starts_with("encsoldier") || editor.starts_with("encimperialsoldier") || editor.starts_with("encstormcloak") || Contains(entry.name,"守卫") || Contains(entry.name,"士兵") || Contains(entry.name,"guard") || Contains(entry.name,"soldier"));
        entry.raceMatch=creatureRaces.contains(entry.race);
        if(auto it=explicitIDs.find(entry.id);it!=explicitIDs.end()) {
            entry.creaturePack=true;entry.dc=it->second.dc;entry.scripted=it->second.scripted;entry.packTags=it->second.names;
            entry.search+=" "+entry.packTags+(entry.dc?" DC Demonic":"");
            if(entry.dc)++dc10_;
        }
    }
    if(catalogStatus10_.empty())catalogStatus10_=fmt::format("清单 {} 个基础 ID；当前加载可解析 {} 个；其中 DC {} 个。",manifest10_,resolved10_,dc10_);
    spdlog::info("Freedom10 catalog: total={}, resolved={}, DC={}",manifest10_,resolved10_,dc10_);
}
void Engine::QueueRandom10(const Action& action,bool soldiers) {
    std::vector<ID> pool;
    auto cat=Catalog();if(!cat)return;
    // Soldier templates can be selected explicitly; auto selection is only a convenience.
    if(soldiers && action.form)pool.push_back(action.form);
    else for(const auto& e:*cat) {
        if(e.kind!=Kind::NPC || e.preset)continue;
        if(soldiers && !e.soldier)continue;
        if(!soldiers && action.scope==0 && !e.creaturePack)continue;
        if(!soldiers && action.scope==1 && !e.dc)continue;
        if(!soldiers && action.scope==2 && !e.raceMatch)continue;
        if(!action.source.empty() && !Contains(e.packTags+" "+e.source,action.source))continue;
        if(!Contains(e.search,action.text))continue;
        // Random generator excludes named unique actors unless explicitly requested.
        if(action.playableOnly && (e.unique || e.scripted))continue;
        pool.push_back(e.id);
    }
    const auto seed=action.seed? action.seed:static_cast<std::uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count());
    auto chosen=RandomBatch10(pool,action.count,seed);
    if(chosen.empty()){Note(soldiers?"没有匹配的士兵模板。先从生成器选择一个士兵基础记录，再使用选定模板按钮。":"当前随机池为空；检查筛选、插件启用或专用清单。");return;}
    Action spawn=action;spawn.op=Op::SpawnActors;spawn.form=chosen.front();spawn.count=static_cast<int>(chosen.size());
    if(soldiers){spawn.recruit=true;spawn.enemy=false;}
    requestedForms10_=std::move(chosen);
    QueueActorSpawn(spawn);requestedForms10_.clear();
    Note(fmt::format("随机池 {} 个基础记录；种子 {}（抽样可重复）。",pool.size(),seed));
}
void Engine::QueueActorSpawn(const Action& action) {
    auto* form=RE::TESForm::LookupByID(action.form);
    if(!form || form->IsDeleted() || !(form->As<RE::TESNPC>() || form->As<RE::TESLevCharacter>())) {
        Note("需要 NPC_ / LVLN 基础记录，不能使用现有角色 RefID 或 Race ID。");return;
    }
    if(!spawnJobs_.empty() || !births10_.empty()){Note("上一批角色仍在生成 / 等待模型。请等待或取消。");return;}
    if(action.recruit) {
        if(!runtime10::Form<RE::TESPackage>(runtime10::Fallback)) {
            Note("缺少本工具的完整跟随后备程序；未生成。请启用 FreedomControlRuntime.esp 后重试。");return;
        }
        // Quest startup/alias filling is asynchronous. A pending quest must not
        // prevent a spawned member from reaching the actor-local fallback.
        EnsureRuntime10();
    }
    if((aura10_ || emptyWorld10_) && (!protectArmy10_ || !action.recruit)){
        Note("清空领域会立即移除本批角色。请先关闭领域，或开启保留军团并勾选自动入队。");return;
    }
    auto* player=RE::PlayerCharacter::GetSingleton();if(!player || !player->GetParentCell())return;
    const int total=std::clamp(action.count,1,MaxSpawnBatch);
    if(spawned_.size()+static_cast<std::size_t>(total)>10000){Note("生成实例登记已达 10000；请清理本工具生成的实例。");return;}
    if(action.recruit && followers_.size()+static_cast<std::size_t>(total)>MaxLegionMembers){Note("军团空位不足；本次未生成任何角色。");return;}
    const ID anchor=MarkerAtPlayer();if(!anchor){Note("生成锚点创建失败。");return;}
    SpawnJob job;job.form=action.form;job.anchor=anchor;job.total=total;job.radius=Finite(action.value[0],300,0,10000);
    job.recruit=action.recruit;job.enemy=action.enemy && !job.recruit;job.applyProfile=action.applyProfile;job.profile=action.profile;job.profile.Normalize();job.epoch=epoch_;
    job.forms=requestedForms10_;if(job.forms.empty())job.forms.assign(static_cast<std::size_t>(total),action.form);
    spawnTotal_=job.total;spawnDone_=spawnCreated_=spawnFailed_=spawnVisible10_=spawnFollowReady14_=spawnFollowPending14_=0;
    spawnJobs_.push_back(std::move(job));dirty_=true;
    Note(fmt::format("已锁定玩家当前位置：生成 {} 个新实例。角色可见与独立跟随分别验证，不再只计数。",total));
}
void Engine::ProcessActorSpawns() {
    if(spawnJobs_.empty())return;
    const auto start=std::chrono::steady_clock::now();
    for(int n=0;n<2 && !spawnJobs_.empty();++n) {
        auto& job=spawnJobs_.front();
        if(job.epoch!=epoch_){CancelActorSpawns();return;}
        auto* marker=Ref(job.anchor);auto* player=RE::PlayerCharacter::GetSingleton();
        if(!marker || !player || !Area10(marker,player)){Note("玩家离开了生成区域；已取消未生成部分。");CancelActorSpawns();return;}
        const ID requested=job.forms.at(static_cast<std::size_t>(job.done));
        auto* f=RE::TESForm::LookupByID(requested);auto* base=f && !f->IsDeleted()?f->As<RE::TESBoundObject>():nullptr;
        // The PLAYER is the live placement source. The marker only stores the requested position.
        // Enabling and waiting for the actor process fixes 'created reference' != 'visible soldier'.
        auto spawned=base?player->PlaceObjectAtMe(base,true):RE::NiPointer<RE::TESObjectREFR>{};
        auto* actor=spawned?spawned->As<RE::Actor>():nullptr;
        if(actor) {
            auto p=marker->GetPosition();auto off=SpawnOffset(job.done,job.total,job.radius,marker->GetAngle().z);
            p.x+=off[0];p.y+=off[1];p.z+=10;
            actor->SetPosition(p,true);actor->Enable(false);actor->EnableAI(true);actor->SetAlpha(1.0f);actor->SetCollision(true);
            auto* actual=actor->GetBaseObject();const ID id=actor->GetFormID();
            if(freeze_){frozen_.try_emplace(id,true);actor->EnableAI(false);}
            spawned_[id]={actual?actual->GetFormID():0,ActorName10(actor)};
            births10_.push_back({id,actual?actual->GetFormID():0,job.recruit,job.applyProfile,job.enemy,job.profile,0,job.epoch});
            // Reserve friendly membership before aura checks, without pretending it already follows.
            if(job.recruit)Follow(actor);
            selected_=id;++spawnCreated_;
            spdlog::info("Freedom10 spawn requested={:08X}, reference={:08X}, base={:08X}, disabled={}, 3D={}, process={}",requested,id,actual?actual->GetFormID():0,actor->IsDisabled(),actor->Get3D()!=nullptr,actor->GetActorRuntimeData().currentProcess!=nullptr);
        } else {
            ++spawnFailed_;if(spawned){spawned->Disable();spawned->SetDelete(true);}
            spdlog::warn("Freedom10 spawn: {:08X} did not produce an Actor; counted as failed",requested);
        }
        ++job.done;++spawnDone_;dirty_=true;
        if(job.done>=job.total){const ID anchor=job.anchor;spawnJobs_.pop_front();if(auto* r=Ref(anchor)){r->Disable();r->SetDelete(true);}}
        if(std::chrono::steady_clock::now()-start>std::chrono::milliseconds(3))break;
    }
}
void Engine::ConfigureEnemy10(RE::Actor* actor) {
    auto* player=RE::PlayerCharacter::GetSingleton();if(!actor || !player || actor==player || followers_.contains(actor->GetFormID()))return;
    if(auto* f=runtime10::Form<RE::TESFaction>(runtime10::Enemies))actor->AddToFaction(f,0);
    actor->SetBaseActorValue(RE::ActorValue::kAggression,1);actor->SetBaseActorValue(RE::ActorValue::kConfidence,4);
    Run("setplayerteammate 0",actor);enemies10_.insert(actor->GetFormID());
    if(!peace_ && !freeze_)actor->StartCombat(player);
}
void Engine::ProcessBirths10(float dt) {
    if(dt<=0 || births10_.empty())return;
    for(auto& b:births10_)b.age+=dt;
    const std::size_t count=std::min<std::size_t>(16,births10_.size());
    for(std::size_t i=0;i<count;++i) {
        auto birth=std::move(births10_.front());births10_.pop_front();
        if(birth.epoch!=epoch_)continue;
        auto* ref=Ref(birth.id);auto* actor=ref?ref->As<RE::Actor>():nullptr;
        const bool exists=actor && !actor->IsDeleted() && actor->GetBaseObject() && actor->GetBaseObject()->GetFormID()==birth.base;
        auto result=CheckBirth10(exists,exists && actor->IsDisabled(),exists && actor->IsDead(false),exists && actor->Get3D(),exists && actor->GetActorRuntimeData().currentProcess,birth.age);
        if(result==BirthResult10::Pending){births10_.push_back(std::move(birth));continue;}
        if(result==BirthResult10::Failed){++spawnFailed_;Note(fmt::format("生成实例 {:08X} 未在 15 秒内形成可用模型 / AI；没有计作成功。可在生成实例列表召回或移除。",birth.id));continue;}
        if(!birth.modelReady14) {
            birth.modelReady14=true;++spawnVisible10_;dirty_=true;
            const auto member=followers_.find(birth.id);
            const bool waiting=member!=followers_.end() && member->second.waiting;
            if(!freeze_ && !waiting && !actor->IsInCombat() && runtime10::CanSnapToGround(actor))actor->MoveToNearestNavmesh();
            spdlog::info("Freedom14 birth model ready: {:08X}, age={:.2f}, army={}, enemy={}",birth.id,birth.age,birth.recruit,birth.enemy);
        }
        if(birth.applyProfile && !birth.profileApplied14) {
            SetMemberStats(actor,birth.profile,true);birth.profileApplied14=true;
            if(auto it=followers_.find(birth.id);it!=followers_.end()){it->second.profile=birth.profile;it->second.profileEnabled=true;}
        }
        if(birth.recruit) {
            auto it=followers_.find(birth.id);
            if(it==followers_.end())Follow(actor);
            it=followers_.find(birth.id);
            if(it!=followers_.end()) {
                // Respect commands issued while a model was loading. Re-running
                // Follow/Prepare(true) every birth poll cancels wait, combat and
                // the package retry cooldown, so maintenance owns reassertions.
                auto& state=it->second;
                if(!freeze_ && !state.waiting)SetFollowPackage(actor,state);
                auto* actual=actor->GetCurrentPackage();
                // The unconditional actor-local fallback is valid even while
                // asynchronous quest startup or alias selection is pending.
                // Read back the actual owned package; do not demand a predicted
                // band pointer or an alias that the fallback intentionally bypasses.
                if(!freeze_ && !state.waiting && !actor->IsInCombat() && runtime10::IsOwnPackage(actual)) {
                    ++spawnFollowReady14_;continue;
                }
            }
            if(birth.age<15.0f){births10_.push_back(std::move(birth));continue;}
            ++spawnFollowPending14_;
            Note(fmt::format("生成实例 {:08X} 模型可用，但跟随包尚未确认；已保留军团登记继续重试。等待 / 冻结 / 战斗 / 生物移动限制也可能使确认延后。",birth.id));
        } else if(birth.enemy)ConfigureEnemy10(actor);

    }
    if(spawnJobs_.empty() && births10_.empty())Note(fmt::format("本批完成：尝试 {}；模型/AI 可用 {}；失败/超时 {}；跟随包已确认 {}，待确认 {}（后续持续重试）。",spawnDone_,spawnVisible10_,spawnFailed_,spawnFollowReady14_,spawnFollowPending14_));
}
void Engine::CancelActorSpawns() {
    for(const auto& job:spawnJobs_)if(auto* marker=Ref(job.anchor)){marker->Disable();marker->SetDelete(true);}
    spawnJobs_.clear();births10_.clear();requestedForms10_.clear();dirty_=true;
}
}
