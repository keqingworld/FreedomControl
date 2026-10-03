#include "PCH.h"
#include "Engine.hpp"
#include "Runtime10.hpp"
#include <RE/B/BGSScene.h>

namespace fc {
namespace {
RE::TESQuest* Quest11(ID id){auto* f=RE::TESForm::LookupByID(id);return f?f->As<RE::TESQuest>():nullptr;}
RE::BGSQuestObjective* Objective11(RE::TESQuest* q,int index) {
    if(q)for(auto* objective:q->objectives)if(objective && objective->index==index)return objective;
    return nullptr;
}
std::string Str11(const char* p){return p?p:"";}
}
bool Engine::IsCoreQuest11(RE::TESQuest* q) const {
    return q && q==runtime10::Form<RE::TESQuest>(runtime10::Quest);
}
int Engine::PersonalSlot11(RE::TESQuest* q) const {
    if(!q)return -1;
    for(int i=0;i<kPersonalQuestSlots11;++i)if(q==runtime10::Form<RE::TESQuest>(kPersonalQuestFirst11+static_cast<ID>(i)))return i;
    return -1;
}
void Engine::SetQuestText11(RE::TESQuest* q,int index,std::string text,bool remember) {
    if(!q)return;
    text=CleanText11(text,index<0?240:512);if(text.empty())return;
    auto* o=index<0?nullptr:Objective11(q,index);if(index>=0&&!o)return;
    auto [it,added]=originalQuestText11_.try_emplace(q->GetFormID());auto& original=it->second;
    if(added)original.name=Str11(q->GetName());
    auto put=[](auto& list,int i,const std::string& t,bool onlyFirst) {
        auto found=std::find_if(list.begin(),list.end(),[&](const auto& p){return p.first==i;});
        if(found==list.end())list.emplace_back(i,t);else if(!onlyFirst)found->second=t;
    };
    if(index<0)q->SetFullName(text.c_str());
    else {put(original.goals,index,Str11(o->displayText.c_str()),true);o->displayText=RE::BSFixedString(text.c_str());}
    if(remember) {auto& edit=questEdits11_[q->GetFormID()];if(index<0)edit.name=text;else put(edit.goals,index,text,false);}
    if(auto* p=RE::PlayerCharacter::GetSingleton())p->GetPlayerFlags().forceQuestTargetRepath=true;
    questDirty11_=dirty_=hudDirty_=true;
}
void Engine::RestoreQuestTexts11() {
    for(const auto& [id,old]:originalQuestText11_)if(auto* q=Quest11(id)) {
        q->SetFullName(old.name.c_str());
        for(const auto& [index,text]:old.goals)if(auto* o=Objective11(q,index))o->displayText=RE::BSFixedString(text.c_str());
    }
    originalQuestText11_.clear();
}
void Engine::UpdateQuestView11() {
    auto* data=RE::TESDataHandler::GetSingleton();if(!data)return;
    auto rows=std::make_shared<std::vector<QuestRow11>>();
    auto make=[&](RE::TESQuest* q,int slot) {
        QuestRow11 r;r.id=q->GetFormID();r.name=Str11(q->GetName());r.editor=Str11(q->GetFormEditorID());
        auto* source=q->GetFile(0);r.source=source?source->fileName:"<runtime>";
        if(r.name.empty())r.name=r.editor.empty()?"[无名称任务]":r.editor;
        r.stage=q->GetCurrentStageID();r.enabled=q->IsEnabled();r.running=q->IsEnabled()&&q->IsRunning();
        r.active=q->IsActive();r.complete=q->IsCompleted();r.blocked=blockedQuests11_.contains(r.id);
        r.protectedRuntime=IsCoreQuest11(q);r.personal=slot>=0;r.personalSlot=slot;
        r.search=r.name+" "+r.editor+" "+r.source+" "+Hex(r.id);return r;
    };
    // Cache native slot IDs once per refresh instead of scanning 64 slots for every quest.
    std::unordered_map<ID,int> slots;
    for(int i=0;i<kPersonalQuestSlots11;++i)if(auto* q=runtime10::Form<RE::TESQuest>(kPersonalQuestFirst11+static_cast<ID>(i)))slots[q->GetFormID()]=i;
    for(auto* q:data->GetFormArray<RE::TESQuest>()) {
        if(!q||q->IsDeleted())continue;
        auto it=slots.find(q->GetFormID());int slot=it==slots.end()?-1:it->second;
        if(slot>=0&&!personal11_[slot])continue;
        rows->push_back(make(q,slot));
    }
    std::sort(rows->begin(),rows->end(),[](const auto& a,const auto& b){if(a.active!=b.active)return a.active>b.active;if(a.running!=b.running)return a.running>b.running;return a.id<b.id;});
    questRows11_=std::move(rows);
    std::string feedback=questDetail11_.feedback;questDetail11_={};questDetail11_.feedback=std::move(feedback);
    if(auto* q=Quest11(questSelected11_)) {
        auto& detail=questDetail11_;detail.valid=true;detail.row=make(q,PersonalSlot11(q));
        for(auto* o:q->objectives)if(o && detail.goals.size()<4096)detail.goals.push_back({o->index,static_cast<int>(o->state.get()),Str11(o->displayText.c_str())});
        if(q->executedStages)for(const auto& stage:*q->executedStages) {if(detail.stages.size()>=4096)break;detail.stages.push_back(stage.data.index);}
        if(q->waitingStages)for(auto* stage:*q->waitingStages) {if(detail.stages.size()>=8192)break;if(stage)detail.stages.push_back(stage->data.index);}
        detail.stages.push_back(q->GetCurrentStageID());
        std::sort(detail.stages.begin(),detail.stages.end());detail.stages.erase(std::unique(detail.stages.begin(),detail.stages.end()),detail.stages.end());
        std::sort(detail.goals.begin(),detail.goals.end(),[](const auto& a,const auto& b){return a.index<b.index;});
    }
    questDirty11_=false;dirty_=true;
}
bool Engine::SyncPersonal11(PersonalQuest11& task,bool restart) {
    if(task.slot<0||task.slot>=kPersonalQuestSlots11)return false;
    auto* q=runtime10::Form<RE::TESQuest>(kPersonalQuestFirst11+static_cast<ID>(task.slot));if(!q)return false;
    const auto id=Hex(q->GetFormID());
    if(restart) {Run("stopquest "+id);q->Reset();syncedPersonal11_[task.slot]=0;}
    SetQuestText11(q,-1,task.title,false);
    for(int i=0;i<kPersonalGoalLimit11;++i)if(auto* o=Objective11(q,(i+1)*10)) {
        (void)o;
        SetQuestText11(q,(i+1)*10,i<static_cast<int>(task.goals.size())?task.goals[i].text:"未使用目标",false);
    }
    const bool running=task.status==PersonalStatus11::Running;
    const bool complete=task.status==PersonalStatus11::Complete;
    if(!running&&!complete) {
        for(int i=0;i<kPersonalGoalLimit11;++i)Run(fmt::format("setobjectivedisplayed {} {} 0",id,(i+1)*10));
        q->data.flags.reset(RE::QuestFlag::kActive);q->AddChange(RE::TESQuest::ChangeFlags::kQuestFlags);
        if(q->IsEnabled())Run("stopquest "+id);
        syncedPersonal11_[task.slot]=task.revision;return true;
    }
    // A finished native journal quest may legitimately be disabled. Restore its
    // display overrides without restarting it or replaying the completion stage.
    if(complete&&q->IsCompleted()) {syncedPersonal11_[task.slot]=task.revision;return true;}
    // Resume of a previously completed slot must clear native completion first.
    if(running&&q->IsCompleted()) {Run("stopquest "+id);q->Reset();}
    if(!q->IsEnabled())q->Start();
    if(!q->IsEnabled()||!q->IsRunning())return false;
    if(!q->GetCurrentStageID())Run("setstage "+id+" 10");
    for(int i=0;i<kPersonalGoalLimit11;++i) {
        const bool exists=i<static_cast<int>(task.goals.size());const auto index=(i+1)*10;
        if(!Objective11(q,index))return false;
        Run(fmt::format("setobjectivedisplayed {} {} {}",id,index,exists?1:0));
        Run(fmt::format("setobjectivefailed {} {} 0",id,index));
        Run(fmt::format("setobjectivecompleted {} {} {}",id,index,exists&&task.goals[i].done?1:0));
    }
    if(complete)Run("setstage "+id+" 100");
    else {q->data.flags.set(RE::QuestFlag::kActive);q->AddChange(RE::TESQuest::ChangeFlags::kQuestFlags);}
    // A revision is acknowledged only once the backing quest has entered the required state.
    if(complete&&!q->IsCompleted())return false;
    syncedPersonal11_[task.slot]=task.revision;
    if(auto* p=RE::PlayerCharacter::GetSingleton())p->GetPlayerFlags().forceQuestTargetRepath=true;
    return true;
}
void Engine::ApplyQuest11(const Action& a) {
    const ID id=a.form?a.form:(a.target?a.target:questSelected11_);
    auto* q=Quest11(id);
    auto report=[&](std::string message){questDetail11_.feedback=message;Note(std::move(message));questDirty11_=dirty_=true;};
    if(a.op==Op::QuestSelect11) {
        questSelected11_=q?id:0;questDetail11_.feedback=q?"已读取任务 ID / 阶段 / 目标。":"这个 ID 不是已加载任务。";
        UpdateQuestView11();return;
    }
    if(a.op==Op::QuestRefresh11){UpdateQuestView11();return;}
    if(a.op==Op::PersonalSave11) {
        lastPersonalSlot11_=-1;lastPersonalRequest11_=a.seed;
        auto task=a.personal11;task.Normalize();int slot=task.slot;
        if(slot<0){for(int i=0;i<kPersonalQuestSlots11;++i)if(!personal11_[i]){slot=i;break;}}
        if(slot<0||slot>=kPersonalQuestSlots11){report("64 个自建任务槽已用完；删除一个自建任务后可继续。");return;}
        if(!runtime10::Form<RE::TESQuest>(kPersonalQuestFirst11+static_cast<ID>(slot))){report("自建任务模板未加载。请更新并勾选本版 FreedomControlRuntime.esp。");return;}
        task.slot=slot;
        if(personal11_[slot]) {
            if(a.personal11.revision!=personal11_[slot]->revision){report("任务进度已变化。请重新读取该任务再保存，避免覆盖自动完成进度。");return;}
            task.revision=personal11_[slot]->revision+1;task.status=personal11_[slot]->status;
        } else {task.revision=1;task.status=PersonalStatus11::Draft;}
        personal11_[slot]=std::move(task);syncedPersonal11_[slot]=0;lastPersonalSlot11_=slot;
        report(fmt::format("已保存自建任务 #{}。点开始进入原版任务日志；可随时暂停、放弃和编辑。",slot+1));return;
    }
    if(a.op==Op::PersonalCommand11||a.op==Op::PersonalGoal11) {
        const int slot=static_cast<int>(a.target);
        if(slot<0||slot>=kPersonalQuestSlots11||!personal11_[slot]){report("请选择现有自建任务。");return;}
        auto& task=*personal11_[slot];
        if(a.op==Op::PersonalGoal11) {
            const auto index=static_cast<std::size_t>(a.form);if(index>=task.goals.size())return;
            task.goals[index].done=a.count!=0;++task.revision;syncedPersonal11_[slot]=0;report("已修改目标进度。");return;
        }
        bool restart=false;
        switch(a.count) {
        case 1:restart=task.status==PersonalStatus11::Complete||task.status==PersonalStatus11::Abandoned;task.status=PersonalStatus11::Running;break;
        case 2:task.status=PersonalStatus11::Paused;break;
        case 3:for(auto& g:task.goals)g.done=true;task.status=PersonalStatus11::Complete;break;
        case 4:task.status=PersonalStatus11::Abandoned;break;
        case 5:for(auto& g:task.goals)g.done=false;task.status=PersonalStatus11::Draft;restart=true;break;
        case 6:
            if(auto* native=runtime10::Form<RE::TESQuest>(kPersonalQuestFirst11+static_cast<ID>(slot))) {
                for(int i=1;i<=kPersonalGoalLimit11;++i)Run(fmt::format("setobjectivedisplayed {} {} 0",Hex(native->GetFormID()),i*10));
                Run("stopquest "+Hex(native->GetFormID()));native->Reset();
                if(auto original=originalQuestText11_.find(native->GetFormID());original!=originalQuestText11_.end()) {
                    native->SetFullName(original->second.name.c_str());for(const auto& [idx,text]:original->second.goals)if(auto* o=Objective11(native,idx))o->displayText=RE::BSFixedString(text.c_str());
                    originalQuestText11_.erase(original);
                }
            }
            personal11_[slot].reset();syncedPersonal11_[slot]=0;report("自建任务已删除，模板槽可再次使用。");return;
        default:return;
        }
        ++task.revision;syncedPersonal11_[slot]=0;const bool applied=SyncPersonal11(task,restart);
        report(applied?"自建任务状态已应用。":"自建任务等待引擎启动/更新，面板会显示原生状态而非假报成功。");return;
    }
    if(!q){report("未选中任务。请在任务中心直接搜索并点击列表，不需要手抄 ID。");return;}
    questSelected11_=id;
    if(IsCoreQuest11(q)){report("这是军团内部运行任务；请用军团页面管理，避免停止后破坏整个随从系统。");return;}
    if(PersonalSlot11(q)>=0){report("这是自建任务模板，请使用“自建任务”页面编辑和改变状态。");return;}
    if(a.op==Op::QuestBlock11) {
        if(a.count){if(blockedQuests11_.size()>=2048){report("已达到 2048 个拒接任务上限。");return;}blockedQuests11_.insert(id);Run("stopquest "+Hex(id));q->data.flags.reset(RE::QuestFlag::kActive);q->AddChange(RE::TESQuest::ChangeFlags::kQuestFlags);}
        else blockedQuests11_.erase(id);
        report(a.count?"已拒接该任务；如果再次启动会被停止。解除拒接不会自动开始任务。":"已解除拒接，可自行点击开始。");return;
    }
    if(blockedQuests11_.contains(id)&&a.op!=Op::QuestText11&&a.op!=Op::QuestTrack11){report("任务仍在拒接列表；先解除拒接再修改阶段或启动。");return;}
    switch(a.op) {
    case Op::QuestStage:Run(fmt::format("setstage {} {}",Hex(id),std::clamp(a.count,0,65535)));report("阶段命令已提交，当前阶段从引擎重新读取；不存在的阶段不会凭空创建。");break;
    case Op::QuestStart:Run("startquest "+Hex(id));report("启动请求已提交，列表显示实际启用/运行状态。");break;
    case Op::QuestStop:Run("stopquest "+Hex(id));report("已请求停止。需要持续防止自动接取时，使用“拒接/持续停止”。");break;
    case Op::QuestReset:q->Reset();report("已调用原生任务重置。已发生的剧情、移动和奖励不会因此完整回滚。");break;
    case Op::QuestComplete:Run("completequest "+Hex(id));report("完成请求已提交，完成标志会重新读取。");break;
    case Op::QuestTrack11:
        if(a.count)q->data.flags.set(RE::QuestFlag::kActive);else q->data.flags.reset(RE::QuestFlag::kActive);
        q->AddChange(RE::TESQuest::ChangeFlags::kQuestFlags);
        if(auto* p=RE::PlayerCharacter::GetSingleton())p->GetPlayerFlags().forceQuestTargetRepath=true;
        report(a.count?"已开启该任务追踪（有显示目标时生效）。":"已取消该任务追踪。");break;
    case Op::QuestObjective11: {
        const int index=std::clamp(a.scope,0,65535);if(!Objective11(q,index)){report("任务中不存在这个目标编号。");break;}
        const auto key=Hex(id);
        if(a.count==0||a.count==1)Run(fmt::format("setobjectivedisplayed {} {} {}",key,index,a.count));
        else if(a.count==2)Run(fmt::format("setobjectivecompleted {} {} 1",key,index));
        else if(a.count==3){Run(fmt::format("setobjectivecompleted {} {} 0",key,index));Run(fmt::format("setobjectivefailed {} {} 0",key,index));Run(fmt::format("setobjectivedisplayed {} {} 1",key,index));}
        else if(a.count==4)Run(fmt::format("setobjectivefailed {} {} 1",key,index));
        report("已提交目标状态修改，列表显示原生目标状态值。");break;
    }
    case Op::QuestText11:
        if(a.scope>=0&&!Objective11(q,a.scope)){report("没有这个原生任务目标。新增自己的目标请使用自建任务。");break;}
        SetQuestText11(q,a.scope,a.text,true);report("任务显示文字已修改并保存到当前 co-save；没有改写原 ESP 或任务脚本。");break;
    default:break;
    }
}
void Engine::MaintainQuests11(float dt) {
    questTimer11_+=dt;
    const bool periodic=questTimer11_>=0.75f;
    if(!periodic&&!questDirty11_)return;
    if(periodic) {
        questTimer11_=0;
        for(ID id:blockedQuests11_)if(auto* q=Quest11(id))if(!IsCoreQuest11(q)&&(q->IsEnabled()||q->IsStarting())) {
            Run("stopquest "+Hex(id));q->data.flags.reset(RE::QuestFlag::kActive);q->AddChange(RE::TESQuest::ChangeFlags::kQuestFlags);++questStops11_;
        }
        // Restore session-local text overrides after a save load or an external reset.
        for(const auto& [id,edit]:questEdits11_)if(auto* q=Quest11(id)) {
            if(!edit.name.empty()&&edit.name!=Str11(q->GetName()))SetQuestText11(q,-1,edit.name,false);
            for(const auto& [index,text]:edit.goals)if(auto* o=Objective11(q,index))if(text!=Str11(o->displayText.c_str()))SetQuestText11(q,index,text,false);
        }
        auto* p=RE::PlayerCharacter::GetSingleton();
        std::unordered_map<ID,int> inventory;bool haveInventory=false;
        for(auto& entry:personal11_)if(entry) {
            auto& task=*entry;
            if(task.status==PersonalStatus11::Running&&p) {
                bool changed=false;
                for(auto& g:task.goals)if(!g.done) {
                    auto* f=RE::TESForm::LookupByID(g.form);bool found=f!=nullptr,same=false,dead=false,base=false;int count=0;float distance=-1;
                    if(g.kind==GoalKind11::CollectItem) {auto* obj=f?f->As<RE::TESBoundObject>():nullptr;found=obj&&obj->IsInventoryObject();if(found){if(!haveInventory){for(const auto& [o,n]:p->GetInventoryCounts())if(o)inventory[o->GetFormID()]=n;haveInventory=true;}if(auto it=inventory.find(g.form);it!=inventory.end())count=std::max(0,it->second);}}
                    else if(g.kind==GoalKind11::DefeatReference){auto* a=f?f->As<RE::Actor>():nullptr;found=a!=nullptr;if(a){dead=a->IsDead(false);base=g.base&&a->GetBaseObject()&&a->GetBaseObject()->GetFormID()==g.base;}}
                    else if(g.kind==GoalKind11::ReachPoint) {
                        auto* target=f?f->As<RE::TESObjectCELL>():nullptr;auto* current=p->GetParentCell();found=target!=nullptr;
                        same=target&&current&&(target==current||(target->IsExteriorCell()&&current->IsExteriorCell()&&g.world&&p->GetWorldspace()&&p->GetWorldspace()->GetFormID()==g.world));
                        auto v=p->GetPosition();distance=Distance(g.position,{v.x,v.y,v.z});
                    }
                    if(AutoGoal11(g,found,count,same,distance,dead,base)){g.done=true;changed=true;}
                }
                if(task.autoFinish&&AllGoalsDone11(task)){task.status=PersonalStatus11::Complete;changed=true;}
                if(changed)++task.revision;
            }
            if(syncedPersonal11_[task.slot]!=task.revision)SyncPersonal11(task,false);
        }
    }
    if(menuOpen_||questDirty11_)UpdateQuestView11();
}
}
