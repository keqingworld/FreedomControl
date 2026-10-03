#include "PCH.h"
#include "Engine.hpp"
#include "Runtime10.hpp"

namespace fc {
void Engine::SaveKernel11(nlohmann::json& root) const {
    auto& j=root["kernel11"];const auto& r=rules11_;
    j["rules"]={{"noBounty",r.noBounty},{"noArrest",r.noArrest},{"guardTruce",r.guardTruce},
        {"playerFreedom14",r.playerFreedom14},{"preserveVoluntary14",r.preserveVoluntary14},
        {"controlGuard",r.controlGuard},{"noAIDriven",r.noAIDriven},{"breakScenes",r.breakScenes},{"breakFurniture",r.breakFurniture},
        {"aimedOwnership",r.aimedOwnership},{"protectClaims",r.protectClaims},{"noSaveWaitLock",r.noSaveWaitLock},
        {"unlimitedTraining",r.unlimitedTraining},{"keepFastTravel",r.keepFastTravel},{"controlMask",r.controlMask},{"guardRadius",r.guardRadius}};
    j["own"]=nlohmann::json::array();j["blocked"]=nlohmann::json::array();j["texts"]=nlohmann::json::array();j["personal"]=nlohmann::json::array();
    for(const auto& [id,rule]:ownRules11_)j["own"].push_back({id,rule.base,rule.unlock});
    for(ID id:blockedQuests11_)j["blocked"].push_back(id);
    for(const auto& [id,text]:questEdits11_) {
        nlohmann::json x={{"id",id},{"name",text.name}};x["goals"]=nlohmann::json::array();
        for(const auto& [index,value]:text.goals)x["goals"].push_back({index,value});
        j["texts"].push_back(std::move(x));
    }
    for(const auto& entry:personal11_)if(entry) {
        const auto& q=*entry;
        nlohmann::json x={{"slot",q.slot},{"revision",q.revision},{"title",q.title},{"status",static_cast<int>(q.status)},{"autoFinish",q.autoFinish}};
        x["goals"]=nlohmann::json::array();
        for(const auto& g:q.goals)x["goals"].push_back({{"text",g.text},{"kind",static_cast<int>(g.kind)},{"form",g.form},{"base",g.base},{"world",g.world},
            {"position",g.position},{"count",g.count},{"radius",g.radius},{"done",g.done}});
        j["personal"].push_back(std::move(x));
    }
}
void Engine::LoadKernel11(const nlohmann::json& root,SKSE::SerializationInterface* serial) {
    if(!root.contains("kernel11"))return;
    const auto& j=root.at("kernel11");
    auto resolve=[&](ID old)->ID {ID current{};return old&&serial&&serial->ResolveFormID(old,current)?current:0;};
    if(j.contains("rules")) {
        const auto& v=j.at("rules");auto& r=rules11_;
        r.noBounty=v.value("noBounty",true);r.noArrest=v.value("noArrest",true);r.guardTruce=v.value("guardTruce",true);
        r.playerFreedom14=v.value("playerFreedom14",true);r.preserveVoluntary14=v.value("preserveVoluntary14",true);
        r.controlGuard=v.value("controlGuard",false);r.noAIDriven=v.value("noAIDriven",false);r.breakScenes=v.value("breakScenes",false);r.breakFurniture=v.value("breakFurniture",false);
        r.aimedOwnership=v.value("aimedOwnership",false);r.protectClaims=v.value("protectClaims",true);r.noSaveWaitLock=v.value("noSaveWaitLock",false);
        r.unlimitedTraining=v.value("unlimitedTraining",false);r.keepFastTravel=v.value("keepFastTravel",false);
        r.controlMask=v.value("controlMask",kKnownControls11);r.guardRadius=v.value("guardRadius",4096.0f);r.Normalize();
    }
    if(j.contains("own"))for(const auto& x:j.at("own")) {
        if(ownRules11_.size()>=50000)break;
        ID id=resolve(x.at(0).get<ID>()),base=resolve(x.at(1).get<ID>());
        if(id&&id!=0x14&&base) {auto [it,inserted]=ownRules11_.emplace(id,OwnRule11{base,x.at(2).get<bool>()});if(inserted)ownRing11_.push_back(id);}
    }
    if(j.contains("blocked"))for(const auto& x:j.at("blocked")) {
        if(blockedQuests11_.size()>=2048)break;
        ID id=resolve(x.get<ID>());
        auto* f=id?RE::TESForm::LookupByID(id):nullptr;auto* q=f?f->As<RE::TESQuest>():nullptr;
        if(q&&!IsCoreQuest11(q)&&PersonalSlot11(q)<0)blockedQuests11_.insert(id);
    }
    if(j.contains("texts"))for(const auto& x:j.at("texts")) {
        if(questEdits11_.size()>=4096)break;
        ID id=resolve(x.at("id").get<ID>());if(!id)continue;
        auto* f=RE::TESForm::LookupByID(id);auto* q=f?f->As<RE::TESQuest>():nullptr;
        if(!q||IsCoreQuest11(q)||PersonalSlot11(q)>=0)continue;
        QuestText11 text;text.name=CleanText11(x.value("name",std::string{}),240);
        if(x.contains("goals"))for(const auto& g:x.at("goals")) {
            if(text.goals.size()>=4096)break;
            int index=g.at(0).get<int>();if(index<0||index>65535)continue;
            text.goals.emplace_back(index,CleanText11(g.at(1).get<std::string>(),512));
        }
        questEdits11_[id]=std::move(text);
    }
    if(j.contains("personal"))for(const auto& x:j.at("personal")) {
        int slot=x.value("slot",-1);if(slot<0||slot>=kPersonalQuestSlots11||personal11_[slot])continue;
        PersonalQuest11 q;q.slot=slot;q.revision=std::max(std::uint64_t{1},x.value("revision",std::uint64_t{1}));
        q.title=x.value("title",std::string("keqing 的自由任务"));q.status=static_cast<PersonalStatus11>(x.value("status",0));q.autoFinish=x.value("autoFinish",true);q.goals.clear();
        if(x.contains("goals"))for(const auto& v:x.at("goals")) {
            if(q.goals.size()>=kPersonalGoalLimit11)break;
            PersonalGoal11 g;
            g.text=v.value("text",std::string("自定义目标"));g.kind=static_cast<GoalKind11>(v.value("kind",0));
            g.form=resolve(v.value("form",ID{}));g.base=resolve(v.value("base",ID{}));g.world=resolve(v.value("world",ID{}));
            if(v.contains("position"))g.position=v.at("position").get<std::array<float,3>>();
            g.count=v.value("count",1);g.radius=v.value("radius",256.0f);g.done=v.value("done",false);g.Normalize();q.goals.push_back(std::move(g));
        }
        q.Normalize();personal11_[slot]=std::move(q);
    }
    syncedPersonal11_.fill(0);questDirty11_=true;
}
}
