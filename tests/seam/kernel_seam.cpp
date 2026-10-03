#include "PCH.h"
#include "Engine.hpp"
#include "Runtime10.hpp"
#include <iostream>
#include <utility>
#include <sstream>
#include <source_location>
namespace fc {
std::vector<std::string> commands11;
namespace law11 {bool g{},j{},request{};void SetPolicy(bool a,bool b){g=a;j=b;}bool TakeClearRequest(){return std::exchange(request,false);}Stats Snapshot(){return {false,0,0,0};}bool Install(){return false;}}
RE::TESObjectREFR* Engine::Ref(ID id)const{return RE::TESForm::LookupByID<RE::TESObjectREFR>(id);}
void Engine::Note(std::string s){notes_.push_back(s);}
bool Engine::IsLegionFriendly(RE::Actor* a)const{return a&&followers_.contains(a->id);}
void Engine::ConfigureClaim(const ClaimState& c){if(auto* cell=RE::TESForm::LookupByID<RE::TESObjectCELL>(c.cell)){cell->owner=RE::PlayerCharacter::GetSingleton()->base;cell->name=c.name;}}
void Engine::Own(RE::TESObjectREFR* r){r->blocked=r->locked=false;r->owner=RE::PlayerCharacter::GetSingleton()->base;ProtectObject11(r,true);}
void Engine::DoorTransit10(RE::TESObjectREFR* r){commands11.push_back("door "+std::to_string(r->id));}
void Engine::Run(std::string_view code,RE::TESObjectREFR* ref){
 commands11.emplace_back(code);if(code=="unlock"&&ref){ref->locked=false;return;}
 std::istringstream in{std::string(code)};std::string cmd,key;in>>cmd>>key;fc::ID id{};try{id=static_cast<fc::ID>(std::stoul(key,nullptr,16));}catch(...){return;}
 auto* q=RE::TESForm::LookupByID<RE::TESQuest>(id);if(!q)return;
 if(cmd=="startquest"){q->Start();return;}if(cmd=="stopquest"){q->enabled=q->running=false;return;}if(cmd=="completequest"){q->data.flags.set(RE::QuestFlag::kCompleted);return;}
 if(cmd=="setstage"){int stage{};in>>stage;if(stage==0||stage==10||stage==100||stage==200){q->stage=stage;if(stage==100)q->data.flags.set(RE::QuestFlag::kCompleted);}return;}
 int index{},value{};in>>index>>value;for(auto* o:q->objectives)if(o->index==index){
  if(cmd=="setobjectivedisplayed"){if(value)o->state.bits=(o->state.bits==2?3:o->state.bits==4?5:1);else o->state.bits=0;}
  if(cmd=="setobjectivecompleted"){if(value)o->state.bits=(o->state.bits==1?3:2);else if(o->state.bits==2||o->state.bits==3)o->state.bits=1;}
  if(cmd=="setobjectivefailed"){if(value)o->state.bits=(o->state.bits==1?5:4);else if(o->state.bits==4||o->state.bits==5)o->state.bits=1;}
 }
}
}
int main(){using namespace fc;int n=0;auto ck=[&](bool b,const std::source_location loc=std::source_location::current()){++n;if(!b)throw std::runtime_error("Kernel seam #"+std::to_string(n)+" at line "+std::to_string(loc.line()));};
 RE::TESDataHandler data;RE::TESDataHandler::instance=&data;RE::TESObjectCELL cell(100),elsewhere(101);RE::TESWorldSpace world(110);
 RE::TESNPC base(200);RE::PlayerCharacter p(0x14);p.base=&base;p.cell=&cell;p.world=&world;p.runtime.currentProcess=&p.process;RE::PlayerCharacter::instance=&p;
 RE::TESFaction f(300);f.gold=200;f.violent=50;Engine e;e.ready_=true;
 e.ClearCrime11(true);ck(f.gold==0&&f.violent==0);ck(!p.arrested);
 auto* controls=RE::ControlMap::GetSingleton();controls->enabled=0x80000000;controls->stored=123;
 e.rules11_=KernelRules11::Preset(1);e.rules11_.playerFreedom14=false;e.TickKernel11(.1f);ck(controls->enabled==0x800007ff);ck(controls->stored==123);ck(law11::g&&law11::j);
 e.menuOpen_=true;controls->enabled=0;e.TickKernel11(.1f);ck(controls->enabled==0);e.menuOpen_=false;
 RE::UI::GetSingleton()->dialogue=true;e.TickKernel11(.1f);ck(controls->enabled==0);RE::UI::GetSingleton()->dialogue=false;
 e.ApplyKernel11(Action{.op=Op::KernelSuspend11,.count=60});e.TickKernel11(.1f);ck(controls->enabled==0);e.suspendUntil11_={};e.TickKernel11(.1f);ck(controls->enabled==0x7ff);
 e.ready_=false;e.TickKernel11(.1f);ck(!law11::g&&!law11::j);e.ready_=true;
 RE::TESObjectDOOR doorBase(400);RE::TESObjectREFR door(401);door.base=&doorBase;door.cell=&cell;door.world=&world;door.locked=door.blocked=true;
 e.ApplyKernel11(Action{.op=Op::OwnUse11,.target=door.id});ck(door.owner==&base&&!door.locked&&!door.blocked);ck(door.activations==1&&!door.lastDefault);ck(e.ownRules11_.contains(door.id));
 e.ApplyKernel11(Action{.op=Op::OwnUse11,.target=door.id,.scope=1});ck(door.lastDefault);
 door.locked=door.blocked=true;door.owner=nullptr;e.MaintainKernel11(.25f,{});ck(!door.locked&&!door.blocked&&door.owner==&base);
 door.disabled=true;door.blocked=true;e.MaintainKernel11(.25f,{});ck(door.blocked);door.disabled=false;
 e.ownRules11_[door.id].base=999;door.locked=true;e.MaintainKernel11(.25f,{});ck(door.locked);e.ownRules11_[door.id].base=doorBase.id;
 e.ApplyKernel11(Action{.op=Op::OwnUse11,.target=door.id,.scope=2});ck(commands11.back()=="door 401");
 RE::Actor guard(501),neutral(502),army(503),enemyGuard(504);for(auto* a:{&guard,&neutral,&army,&enemyGuard}){a->cell=&cell;a->world=&world;a->base=&base;a->runtime.currentProcess=&a->process;}
 guard.guard=enemyGuard.guard=true;guard.combat=enemyGuard.combat=true;guard.runtime.currentCombatTarget={&p};enemyGuard.runtime.currentCombatTarget={&p};e.enemies10_.insert(enemyGuard.id);
 guard.runtime.boolFlags.set(RE::Actor::BOOL_FLAGS::kCrimeSearch);guard.runtime.boolBits.set(RE::Actor::BOOL_BITS::kForceGreetingPlayer);
 neutral.combat=true;neutral.runtime.currentCombatTarget={&p};e.MaintainKernel11(.25f,{guard.id,neutral.id,enemyGuard.id});ck(!guard.combat&&guard.dialoguesStopped>0);ck(neutral.combat);ck(enemyGuard.combat);
 guard.combat=true;guard.runtime.currentCombatTarget={&neutral};e.MaintainKernel11(.25f,{guard.id});ck(guard.combat);
 e.rules11_.breakScenes=true;RE::BGSScene sc(600);p.scene=&sc;e.TickKernel11(.5f);ck(!p.scene);
 e.rules11_.breakFurniture=true;p.furniture={&door};e.TickKernel11(.5f);ck(!p.furniture.p);
 e.ApplyKernel11(Action{.op=Op::Rest11,.count=2});ck(e.resting11_);e.TickKernel11(.1f);ck(!e.resting11_&&p.info.sleepSeconds==0);
 p.info.sleepSeconds=3600;e.ApplyKernel11(Action{.op=Op::Rest11,.count=4});ck(!e.resting11_&&p.info.sleepSeconds==3600);p.info.sleepSeconds=0;
 RE::TESQuest core(0xfe001800),q(700);data.keys[{"FreedomControlRuntime.esp",0x800}]=&core;q.editor="MQTest";q.name="测试任务";
 RE::BGSQuestObjective objective;objective.index=20;objective.displayText="原目标";q.objectives.push_back(&objective);
 RE::TESQuestStage st;st.data.index=10;std::vector<RE::TESQuestStage> stages{st};q.executedStages=&stages;
 e.UpdateQuestView11();ck(e.questRows11_->size()==2);e.ApplyQuest11(Action{.op=Op::QuestSelect11,.form=q.id});ck(e.questDetail11_.valid&&e.questDetail11_.row.editor=="MQTest");ck(e.questDetail11_.goals.size()==1);ck(e.questDetail11_.stages.size()==2);
 e.ApplyQuest11(Action{.op=Op::QuestStart,.form=q.id});ck(q.enabled&&q.running);
 e.ApplyQuest11(Action{.op=Op::QuestStage,.form=q.id,.count=10});ck(q.stage==10);
 e.ApplyQuest11(Action{.op=Op::QuestStage,.form=q.id,.count=999});ck(q.stage==10);
 e.ApplyQuest11(Action{.op=Op::QuestTrack11,.form=q.id});ck(q.IsActive());
 e.ApplyQuest11(Action{.op=Op::QuestObjective11,.form=q.id,.count=1,.scope=20});ck(objective.state.bits==1);
 e.ApplyQuest11(Action{.op=Op::QuestObjective11,.form=q.id,.count=2,.scope=20});ck(objective.state.bits==3);
 auto before=commands11.size();e.ApplyQuest11(Action{.op=Op::QuestText11,.form=q.id,.text="keqing; stopquest MQ101",.scope=-1});ck(q.name=="keqing; stopquest MQ101");ck(commands11.size()==before);
 e.ApplyQuest11(Action{.op=Op::QuestText11,.form=q.id,.text="自选目标",.scope=20});ck(objective.displayText=="自选目标");
 e.ApplyQuest11(Action{.op=Op::QuestBlock11,.form=q.id});ck(!q.enabled&&e.blockedQuests11_.contains(q.id));q.Start();e.MaintainQuests11(1);ck(!q.enabled);
 e.ApplyQuest11(Action{.op=Op::QuestStart,.form=q.id});ck(!q.enabled);e.ApplyQuest11(Action{.op=Op::QuestBlock11,.form=q.id,.count=0});ck(!e.blockedQuests11_.contains(q.id));
 core.Start();e.ApplyQuest11(Action{.op=Op::QuestStop,.form=core.id});ck(core.enabled);
 e.RestoreQuestTexts11();ck(q.name=="测试任务"&&objective.displayText=="原目标");
 std::array<RE::TESQuest,64> backing;std::array<std::array<RE::BGSQuestObjective,8>,64> goals;
 for(int i=0;i<64;++i){backing[i].id=0xfe001900+static_cast<ID>(i);RE::registry[backing[i].id]=&backing[i];data.keys[{"FreedomControlRuntime.esp",0x900+static_cast<ID>(i)}]=&backing[i];backing[i].name="template";for(int j=0;j<8;++j){goals[i][j].index=(j+1)*10;goals[i][j].displayText="placeholder";backing[i].objectives.push_back(&goals[i][j]);}}
 Action save;save.op=Op::PersonalSave11;save.seed=123;save.personal11.title="自由探险";save.personal11.goals[0].text="随我心情完成";e.ApplyQuest11(save);ck(e.personal11_[0]&&e.lastPersonalSlot11_==0&&e.lastPersonalRequest11_==123);
 e.ApplyQuest11(Action{.op=Op::PersonalCommand11,.target=0,.count=1});ck(e.personal11_[0]->status==PersonalStatus11::Running);ck(backing[0].enabled&&backing[0].stage==10&&goals[0][0].state.bits==1);ck(backing[0].name=="自由探险");
 e.ApplyQuest11(Action{.op=Op::PersonalGoal11,.target=0,.form=0,.count=1});e.MaintainQuests11(1);ck(e.personal11_[0]->status==PersonalStatus11::Complete&&backing[0].IsCompleted());
 // Completed native quests are often disabled by the engine; re-sync must not restart.
 backing[0].enabled=backing[0].running=false;auto startsBefore=backing[0].starts;
 e.syncedPersonal11_[0]=0;e.MaintainQuests11(1);ck(backing[0].starts==startsBefore);ck(!backing[0].enabled&&backing[0].IsCompleted());
 e.ApplyQuest11(Action{.op=Op::PersonalCommand11,.target=0,.count=2});
 e.ApplyQuest11(Action{.op=Op::PersonalCommand11,.target=0,.count=1});ck(backing[0].enabled&&!backing[0].IsCompleted());
 e.ApplyQuest11(Action{.op=Op::PersonalCommand11,.target=0,.count=5});ck(e.personal11_[0]->status==PersonalStatus11::Draft&&!e.personal11_[0]->goals[0].done);ck(!backing[0].enabled);
 save.personal11.slot=0;save.personal11.revision=1;e.ApplyQuest11(save);ck(e.lastPersonalSlot11_==-1);ck(e.personal11_[0]->title=="自由探险");
 save.personal11=*e.personal11_[0];save.personal11.goals[0].kind=GoalKind11::CollectItem;save.personal11.goals[0].form=doorBase.id;doorBase.inventory=true;save.personal11.goals[0].count=3;e.ApplyQuest11(save);ck(e.lastPersonalSlot11_==0);
 e.ApplyQuest11(Action{.op=Op::PersonalCommand11,.target=0,.count=1});p.inventory[&doorBase]=2;e.MaintainQuests11(1);ck(!e.personal11_[0]->goals[0].done);p.inventory[&doorBase]=3;e.MaintainQuests11(1);ck(e.personal11_[0]->goals[0].done&&backing[0].IsCompleted());
 save.personal11=PersonalQuest11{};save.personal11.title="自由探险";e.ApplyQuest11(save);e.UpdateQuestView11();int slots=0;for(auto& row:*e.questRows11_)if(row.personal){++slots;ck(row.personalSlot==0||row.personalSlot==1);}ck(slots==2);
 e.ApplyQuest11(Action{.op=Op::PersonalCommand11,.target=1,.count=2});ck(e.personal11_[1]->status==PersonalStatus11::Paused);e.ApplyQuest11(Action{.op=Op::PersonalCommand11,.target=1,.count=6});ck(!e.personal11_[1]&&backing[1].name=="template");
 // Production save/load implementation roundtrip against the explicit test value tree.
 e.ApplyQuest11(Action{.op=Op::QuestBlock11,.form=q.id});e.ApplyQuest11(Action{.op=Op::QuestText11,.form=q.id,.text="保留文字",.scope=-1});
 nlohmann::json root;e.SaveKernel11(root);ck(root.at("kernel11").at("personal").size()==1);Engine restored;SKSE::SerializationInterface serial;restored.LoadKernel11(root,&serial);ck(restored.personal11_[0]&&restored.personal11_[0]->title=="自由探险");ck(restored.blockedQuests11_.contains(q.id));ck(restored.ownRules11_.contains(door.id));ck(restored.questEdits11_.at(q.id).name=="保留文字");
 serial.mapped[door.id]=12345;Engine remapped;remapped.LoadKernel11(root,&serial);ck(remapped.ownRules11_.contains(12345)&&!remapped.ownRules11_.contains(door.id));serial.mapped[doorBase.id]=0;Engine missing;missing.LoadKernel11(root,&serial);ck(!missing.ownRules11_.contains(12345));ck(missing.personal11_[0]->goals[0].form==0);
 restored.RestoreQuestTexts11();restored.ResetKernel11();ck(!restored.personal11_[0]&&restored.blockedQuests11_.empty()&&restored.ownRules11_.empty());ck(restored.rules11_.controlGuard&&!law11::g&&!law11::j);
 // Exercise the production player-freedom gateway, not just the pure intent policy.
 {
  Engine freedom;RE::Actor chosen(0x1401),intruder(0x1402);RE::BGSScene scene(0x1403);RE::TESQuest sceneQuest(0x1404);
  chosen.base=intruder.base=&base;chosen.cell=intruder.cell=&cell;chosen.runtime.currentProcess=&chosen.process;intruder.runtime.currentProcess=&intruder.process;
  scene.parentQuest=&sceneQuest;sceneQuest.Start();
  auto* ui=RE::UI::GetSingleton();auto* topic=RE::MenuTopicManager::GetSingleton();auto* queue=RE::UIMessageQueue::GetSingleton();auto* pick=RE::CrosshairPickData::GetSingleton();
  auto reset=[&]{
   freedom.ResetKernel11();freedom.ready_=true;freedom.menuOpen_=false;freedom.inputBlocked_=false;freedom.pauseLease_=PauseLease{};
   freedom.rules11_.breakScenes=false;freedom.rules11_.breakFurniture=false;freedom.rules11_.aimedOwnership=false;
   *ui=RE::UI{};queue->hides=0;queue->lastMenu.clear();topic->speaker={};pick->target={&chosen};
   p.dead=false;p.playerFlags={};p.scene=nullptr;p.furniture={};p.runtime.boolFlags.bits=p.runtime.boolBits.bits=0;p.dialoguesStopped=0;
   chosen.dead=chosen.deleted=chosen.disabled=false;chosen.dialoguesStopped=0;chosen.runtime.boolBits.bits=0;
   intruder.dialoguesStopped=0;intruder.runtime.boolBits.bits=0;controls->enabled=0x7ff;controls->stored=123;
  };
  auto forced=[&](RE::Actor& who){ui->dialogue=true;topic->speaker={&who};p.scene=&scene;p.SetAIDriven(true);controls->enabled=0x80000000;p.runtime.boolFlags.set(RE::Actor::BOOL_FLAGS::kMovementBlocked);p.runtime.boolFlags.set(RE::Actor::BOOL_FLAGS::kAttackingDisabled);p.runtime.boolFlags.set(RE::Actor::BOOL_FLAGS::kCastingDisabled);p.runtime.boolFlags.set(RE::Actor::BOOL_FLAGS::kScenePackage);p.runtime.boolBits.set(RE::Actor::BOOL_BITS::kHeadingFixed);who.runtime.boolBits.set(RE::Actor::BOOL_BITS::kForceGreetingPlayer);};
  reset();forced(intruder);freedom.TickKernel11(.1f);
  ck(queue->hides==1&&queue->lastMenu=="Dialogue Menu");ck(intruder.dialoguesStopped==1&&p.dialoguesStopped>0);
  ck(!p.scene&&!p.GetPlayerFlags().aiControlledPackage);ck(controls->enabled==0x800007ff&&controls->stored==123);
  ck(!p.runtime.boolFlags.any(RE::Actor::BOOL_FLAGS::kMovementBlocked)&&!p.runtime.boolBits.any(RE::Actor::BOOL_BITS::kHeadingFixed));
  ck(!p.runtime.boolFlags.any(RE::Actor::BOOL_FLAGS::kAttackingDisabled)&&!p.runtime.boolFlags.any(RE::Actor::BOOL_FLAGS::kCastingDisabled));
  ck(!intruder.runtime.boolBits.any(RE::Actor::BOOL_BITS::kForceGreetingPlayer));ck(sceneQuest.IsRunning()&&!sceneQuest.IsCompleted());
  ck(freedom.rejectedDialogues14_==1&&freedom.releasedScenes14_==1);
  freedom.TickKernel11(.01f);ck(queue->hides==1); // Asynchronous hide readback, rate-limited.
  freedom.freedomActionAt14_={};freedom.TickKernel11(.01f);ck(queue->hides==2);
  ui->dialogue=false;topic->speaker={};freedom.freedomActionAt14_={};freedom.TickKernel11(.1f);ck(queue->hides==2);

  reset();freedom.RecordPlayerActivation14();forced(chosen);freedom.TickKernel11(.1f);
  ck(queue->hides==0&&p.scene==&scene);ck(chosen.dialoguesStopped==0&&p.dialoguesStopped==0);
  ck(controls->enabled==0x80000000&&p.GetPlayerFlags().aiControlledPackage); // Preserve deliberate dialogue isolation.
  freedom.TickKernel11(.5f);ck(queue->hides==0&&p.scene==&scene);
  topic->speaker={&intruder};freedom.TickKernel11(.1f);ck(queue->hides==1&&!p.scene); // Permission is actor-bound.
  reset();freedom.RecordPlayerActivation14();forced(intruder);freedom.TickKernel11(.1f);ck(queue->hides==1);
  reset();freedom.RecordPlayerActivation14();forced(chosen);freedom.TickKernel11(.1f);ui->dialogue=false;p.scene=nullptr;chosen.runtime.boolBits.reset(RE::Actor::BOOL_BITS::kForceGreetingPlayer);freedom.TickKernel11(.1f);
  forced(chosen);freedom.TickKernel11(.1f);ck(queue->hides==1); // Closing/reopening revokes consent.
  reset();freedom.RecordPlayerActivation14();freedom.rules11_.preserveVoluntary14=false;forced(chosen);freedom.TickKernel11(.1f);ck(queue->hides==1);
  reset();p.scene=&scene;freedom.TickKernel11(.1f);ck(!p.scene&&queue->hides==0&&sceneQuest.IsRunning());
  reset();topic->speaker={&intruder};intruder.runtime.boolBits.set(RE::Actor::BOOL_BITS::kForceGreetingPlayer);freedom.TickKernel11(.1f);ck(queue->hides==1&&intruder.dialoguesStopped==1); // Pending force-greet before UI opening.
  reset();topic->speaker={&intruder};freedom.TickKernel11(.1f);ck(queue->hides==0&&intruder.dialoguesStopped==0); // Ordinary ambient hello is untouched.

  reset();freedom.rules11_.playerFreedom14=false;forced(chosen);freedom.TickKernel11(.5f);ck(queue->hides==0&&p.scene==&scene&&controls->enabled==0x80000000);
  // Runtime guards must leave asynchronous UI and scene state untouched.
  for(int mode=0;mode<8;++mode){
   reset();forced(intruder);
   if(mode==0)freedom.ready_=false;
   if(mode==1)p.dead=true;
   if(mode==2)p.playerFlags.isLoading=true;
   if(mode==3)ui->paused=true;
   if(mode==4)freedom.menuOpen_=true;
   if(mode==5)freedom.inputBlocked_=true;
   if(mode==6)freedom.pauseLease_.Update(true,false);
   if(mode==7)freedom.ApplyKernel11(Action{.op=Op::KernelSuspend11,.count=60});
   freedom.TickKernel11(.5f);ck(queue->hides==0&&p.scene==&scene&&controls->enabled==0x80000000);
  }
  reset();freedom.RecordPlayerActivation14();freedom.ApplyKernel11(Action{.op=Op::KernelSuspend11,.count=60});freedom.suspendUntil11_={};forced(chosen);freedom.TickKernel11(.1f);ck(queue->hides==1);
  // Rejected activation contexts cannot grant a later dialogue permission.
  for(int mode=0;mode<18;++mode){
   reset();
   if(mode==0)freedom.ready_=false;
   if(mode==1)freedom.rules11_.playerFreedom14=false;
   if(mode==2)freedom.rules11_.preserveVoluntary14=false;
   if(mode==3)freedom.menuOpen_=true;
   if(mode==4)freedom.inputBlocked_=true;
   if(mode==5)p.dead=true;
   if(mode==6)p.playerFlags.isLoading=true;
   if(mode==7)ui->paused=true;
   if(mode==8)ui->dialogue=true;
   if(mode==9)controls->enabled&=~(1u<<2);
   if(mode==10)chosen.dead=true;
   if(mode==11)chosen.deleted=true;
   if(mode==12)chosen.disabled=true;
   if(mode==13)pick->target={&p};
   if(mode==14)ui->item=true;
   if(mode==15)ui->modal=true;
   if(mode==16)ui->application=true;
   if(mode==17)freedom.suspendUntil11_=std::chrono::steady_clock::now()+std::chrono::seconds(60);
   freedom.RecordPlayerActivation14();
   freedom.ready_=true;freedom.rules11_.playerFreedom14=freedom.rules11_.preserveVoluntary14=true;freedom.menuOpen_=false;freedom.inputBlocked_=false;
   p.dead=false;p.playerFlags.isLoading=false;ui->paused=ui->item=ui->modal=ui->application=false;freedom.suspendUntil11_={};chosen.dead=chosen.deleted=chosen.disabled=false;
   forced(chosen);freedom.TickKernel11(.1f);ck(queue->hides==1);
  }
  reset();pick->target={&door};freedom.RecordPlayerActivation14();forced(chosen);freedom.TickKernel11(.1f);ck(queue->hides==1);
  // A later door/item activation cancels a still-pending actor token.
  reset();freedom.RecordPlayerActivation14();pick->target={&door};freedom.RecordPlayerActivation14();forced(chosen);freedom.TickKernel11(.1f);ck(queue->hides==1);
  // Menu close observation invalidates consent even with no Tick between close and reopen.
  reset();freedom.RecordPlayerActivation14();forced(chosen);freedom.TickKernel11(.1f);ck(queue->hides==0);
  freedom.RecordPlayerDialogueClosed14();forced(chosen);freedom.TickKernel11(.1f);ck(queue->hides==1);
  // A new activation after the close generation obtains a fresh, specific lease.
  reset();freedom.RecordPlayerDialogueClosed14();freedom.RecordPlayerActivation14();forced(chosen);freedom.TickKernel11(.1f);ck(queue->hides==0);
  // No controls/dialogue/scene mutation beneath a non-pausing native menu.
  for(int mode=0;mode<3;++mode){
   reset();p.scene=&scene;controls->enabled=0x80000000;p.SetAIDriven(true);
   if(mode==0)ui->item=true;
   if(mode==1)ui->modal=true;
   if(mode==2)ui->application=true;
   freedom.TickKernel11(.5f);ck(queue->hides==0&&p.scene==&scene&&controls->enabled==0x80000000&&p.GetPlayerFlags().aiControlledPackage);
  }
  // Pending input cannot survive a paused/loading/dead/F8/native-menu boundary.
  for(int mode=0;mode<8;++mode){
   reset();freedom.RecordPlayerActivation14();
   if(mode==0)ui->paused=true;
   if(mode==1)p.playerFlags.isLoading=true;
   if(mode==2)p.dead=true;
   if(mode==3)freedom.menuOpen_=true;
   if(mode==4)ui->item=true;
   if(mode==5)ui->modal=true;
   if(mode==6)ui->application=true;
   if(mode==7)freedom.ready_=false;
   freedom.TickKernel11(.1f);
   ui->paused=ui->item=ui->modal=ui->application=false;p.playerFlags.isLoading=p.dead=false;freedom.menuOpen_=false;freedom.ready_=true;
   forced(chosen);freedom.TickKernel11(.1f);ck(queue->hides==1);
  }
  // A voluntary conversation can be paused then resumed without being cancelled.
  reset();freedom.RecordPlayerActivation14();forced(chosen);freedom.TickKernel11(.1f);ui->paused=true;freedom.TickKernel11(.1f);ui->paused=false;freedom.TickKernel11(.1f);ck(queue->hides==0&&p.scene==&scene);
  // A different scene does not inherit consent or stop the accepted dialogue.
  reset();freedom.RecordPlayerActivation14();forced(chosen);freedom.TickKernel11(.1f);
  RE::BGSScene unrelated(0x1405);p.scene=&unrelated;freedom.TickKernel11(.1f);ck(!p.scene&&queue->hides==0&&p.dialoguesStopped==0&&chosen.dialoguesStopped==0);
  // Unsolicited control recovery only releases the precise restrained state.
  reset();p.SetLifeState(RE::ACTOR_LIFE_STATE::kRestrained);freedom.TickKernel11(.1f);ck(p.GetLifeState()==RE::ACTOR_LIFE_STATE::kAlive);
  // Save the feature switches, never runtime intent. Older saves get explicit defaults.
  reset();freedom.rules11_.playerFreedom14=false;freedom.rules11_.preserveVoluntary14=false;nlohmann::json saved;freedom.SaveKernel11(saved);
  Engine loaded;loaded.LoadKernel11(saved,&serial);ck(!loaded.rules11_.playerFreedom14&&!loaded.rules11_.preserveVoluntary14);
  freedom.rules11_.playerFreedom14=freedom.rules11_.preserveVoluntary14=true;freedom.SaveKernel11(saved);loaded.LoadKernel11(saved,&serial);ck(loaded.rules11_.playerFreedom14&&loaded.rules11_.preserveVoluntary14);
  nlohmann::json oldSave;oldSave["kernel11"]["rules"]={{"controlGuard",false}};loaded.rules11_.playerFreedom14=loaded.rules11_.preserveVoluntary14=false;loaded.LoadKernel11(oldSave,&serial);ck(loaded.rules11_.playerFreedom14&&loaded.rules11_.preserveVoluntary14);
  reset();freedom.RecordPlayerActivation14();freedom.SaveKernel11(saved);Engine fromSave;fromSave.LoadKernel11(saved,&serial);fromSave.ready_=true;forced(chosen);fromSave.TickKernel11(.1f);ck(queue->hides==1);
  reset();freedom.RecordPlayerActivation14();freedom.ResetKernel11();forced(chosen);freedom.TickKernel11(.1f);ck(queue->hides==1);
  reset();
 }
 std::cout<<"PASS: "<<n<<" production Kernel11 / PlayerFreedom14 / QuestCenter11 / KernelSave11 control assertions with real fmt and explicit engine/JSON value doubles. NOT Windows, nlohmann parser, SKSE or game validation.\n";
}
