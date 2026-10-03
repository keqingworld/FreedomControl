#include "PCH.h"
#include "Engine.hpp"
#include "Runtime10.hpp"
#include <iostream>
#include <stdexcept>
#include <source_location>
#include "RuntimeESPFixture15.hpp"

// Test implementations of the pre-existing Engine side effects called by Legion.cpp.
namespace fc {
static Engine* eventEngine10{};
Engine& Engine::Get(){return *eventEngine10;}
void Engine::Submit(Action a){if(!a.epoch)a.epoch=epoch_.load();queue_.push_back(std::move(a));}
std::shared_ptr<const std::vector<Entry>> Engine::Catalog()const{return catalog_;}
RE::TESObjectREFR* Engine::Ref(ID id)const{return RE::TESForm::LookupByID<RE::TESObjectREFR>(id);}
void Engine::Note(std::string s){notes_.push_back(std::move(s));}
void Engine::Run(std::string_view s,RE::TESObjectREFR* ref){if(auto* a=ref?ref->As<RE::Actor>():nullptr){if(s=="setplayerteammate 1")a->teammate=true;else if(s=="setplayerteammate 0")a->teammate=false;}}
bool Engine::SetValue(RE::Actor* a,const std::string&,float value,bool){if(a)a->SetBaseActorValue(RE::ActorValue::kSpeedMult,value);return a!=nullptr;}
ID Engine::MarkerAtPlayer(ID){static RE::TESBoundObject markerBase(0x3b);auto p=RE::PlayerCharacter::GetSingleton()->PlaceObjectAtMe(&markerBase,true);return p->GetFormID();}
}
int main(){
    int checks=0;auto check=[&](bool ok,std::source_location where=std::source_location::current()){++checks;if(!ok)throw std::runtime_error("seam assertion #"+std::to_string(checks)+" line "+std::to_string(where.line()));};
    RE::TESObjectCELL cell(100),other(101);RE::TESNPC base(200);RE::TESFaction guardFaction(300);RE::BGSScene scene(400);
    RE::PlayerCharacter player(0x14);player.cell=&cell;player.base=&base;RE::PlayerCharacter::instance=&player;
    RE::ProcessLists processes;RE::ProcessLists::instance=&processes;
    RE::Actor guard(501),enemy(502),outsider(503);for(auto* a:{&guard,&enemy,&outsider}){a->base=&base;a->cell=&cell;a->runtime.currentProcess=&a->process;}
    guard.pos={0,1000,0};guard.factions[&guardFaction]=5;guard.scene=&scene;guard.sit=RE::SIT_SLEEP_STATE::kIsSitting;
    guard.runtime.boolFlags.set(RE::Actor::BOOL_FLAGS::kMovementBlocked);
    RE::TESDataHandler data;RE::TESDataHandler::instance=&data;
    RE::TESFile ownFile;std::strcpy(ownFile.fileName,"FreedomControlRuntime.esp");
    RE::TESQuest quest(0xfe001800);RE::TESFaction members(0xfe001801),enemies(0xfe001802),dragons(0xfe001803);
    RE::TESGlobal band(0xfe001804);RE::TESObjectREFR parking(0xfe001806);RE::TESPackage follow;follow.id=0xfe00180a;follow.file=&ownFile;
    RE::SpellItem power(0xfe001808);
    auto bind=[&](fc::ID local,RE::TESForm* form){data.keys[{"FreedomControlRuntime.esp",local}]=form;};
    bind(fc::runtime10::Quest,&quest);bind(fc::runtime10::Members,&members);bind(fc::runtime10::Enemies,&enemies);bind(fc::runtime10::Dragons,&dragons);bind(fc::runtime10::Band,&band);bind(fc::runtime10::Parking,&parking);bind(fc::runtime10::Fallback,&follow);bind(fc::runtime10::Spell,&power);
    quest.assignedPackage=&follow;
    fc::Engine engine;fc::eventEngine10=&engine;engine.Follow(&guard);
    check(quest.starts==1);check(engine.runtimeReady10_);
    check(engine.followers_.contains(guard.id));check(guard.teammate);check(!guard.scene);check(guard.factions[&guardFaction]==-1);
    check(!guard.runtime.boolFlags.any(RE::Actor::BOOL_FLAGS::kMovementBlocked));check(guard.getups>0);
    // Persistently seated actor: bounded get-up then force exit. Uses real Legion.cpp.
    for(int i=0;i<40;++i)engine.MaintainLegion(.25f,{guard.id});
    check(guard.sit==RE::SIT_SLEEP_STATE::kNormal);check(guard.currentPackage!=nullptr);
    check(!guard.currentPackage->packData.packFlags.any(RE::PACKAGE_DATA::GeneralFlag::kIgnoreCombat));
    check(!guard.currentPackage->packData.packFlags.any(RE::PACKAGE_DATA::GeneralFlag::kNoCombatAlert));
    // Legitimate enemy combat is NOT stopped on every maintenance pass (old bug).
    enemy.hostile=true;enemy.pos={100,100,0};enemy.runtime.currentCombatTarget={&player};
    engine.MaintainLegion(.25f,{guard.id,enemy.id});check(guard.combat);check(guard.runtime.currentCombatTarget.p==&enemy);
    int stopped=guard.combatStops;engine.MaintainLegion(.25f,{guard.id,enemy.id});check(guard.combatStops==stopped);
    // A different neutral actor is never selected simply because it is nearby.
    guard.combat=true;guard.runtime.currentCombatTarget={&outsider};enemy.hostile=false;enemy.runtime.currentCombatTarget={};
    engine.MaintainLegion(.25f,{guard.id,enemy.id,outsider.id});check(!guard.combat);
    // Player teammate safety and peace precedence.
    guard.combat=true;guard.runtime.currentCombatTarget={&player};engine.MaintainLegion(.25f,{guard.id});check(!guard.combat);
    enemy.hostile=true;engine.peace_=true;engine.MaintainLegion(.25f,{guard.id,enemy.id});check(!guard.combat);
    engine.peace_=false;engine.autoFight_=false;
    // Wrong-way patrol is corrected despite nonzero movement.
    guard.pos={0,1500,0};int moved=guard.moves;
    for(int i=0;i<55;++i){guard.pos.y+=25;guard.currentPackage=nullptr;engine.MaintainLegion(.25f,{guard.id});}
    check(guard.moves>moved);
    // Explicit take-control/recruit path must also release an older wait/pacify order.
    engine.followers_.at(guard.id).waiting=true; engine.pacified_[guard.id]=2; guard.ai=false;
    engine.Follow(&guard);
    check(!engine.followers_.at(guard.id).waiting); check(!engine.pacified_.contains(guard.id)); check(guard.ai);
    check(engine.followers_.at(guard.id).oldFactions.at(guardFaction.id)==5);
    // Army orders update unloaded member state too, rather than silently skipping them.
    engine.followers_[9999].id=9999;
    fc::Action order; order.count=1; engine.ApplyLegionOrder(order); check(engine.followers_.at(9999).waiting);
    order.count=0; engine.ApplyLegionOrder(order); check(!engine.followers_.at(9999).waiting);
    engine.followers_.erase(9999);
    engine.Dismiss(guard.id);check(!guard.teammate);check(guard.factions[&guardFaction]==5);check(!engine.followers_.contains(guard.id));
    // Death clears shared protected flags only for the call; restores template afterwards.
    base.actorData.actorBaseFlags.set(RE::ACTOR_BASE_DATA::Flag::kEssential);
    engine.ForceDeath(&guard,true);engine.MaintainRemovals(.5f);check(guard.dead);check(!engine.removed_.contains(guard.id));
    check(base.actorData.actorBaseFlags.any(RE::ACTOR_BASE_DATA::Flag::kEssential));
    // Rejected death is not called successful; explicit fallback removes and suppresses.
    outsider.refuseDeath=true;engine.ForceDeath(&outsider,true);
    for(int i=0;i<5;++i)engine.MaintainRemovals(.5f);
    check(!outsider.dead);check(outsider.disabled && outsider.deleted && !outsider.collision);check(outsider.alpha==0);check(engine.removed_.contains(outsider.id));
    outsider.disabled=false;outsider.deleted=false;engine.MaintainRemovals(.5f);check(outsider.disabled);
    // Same base, different reference is not removed; player is excluded.
    check(!enemy.disabled);engine.RemoveReference(&player);check(!player.disabled);
    // Anchored batch creates requested additional instances, not moves of originals.
    fc::Action spawn{.op=fc::Op::SpawnActors,.form=base.id,.count=4,.value={0,0,0,0},.text={},.epoch=1,.source={},.profile={},.recruit=true};
    engine.QueueActorSpawn(spawn);for(int i=0;i<5;++i)engine.ProcessActorSpawns();
    check(engine.spawnCreated_==4);check(engine.spawnDone_==4);check(engine.spawnJobs_.empty());check(engine.followers_.size()==4);
    for(auto [id,entry]:engine.spawned_){auto* r=engine.Ref(id);check(r!=&guard && r!=&player);check(r->pos.x==player.pos.x && r->pos.y==player.pos.y);}
    check(engine.spawnVisible10_==0);engine.ProcessBirths10(0);check(engine.spawnVisible10_==0);
    for(int i=0;i<5;++i)engine.ProcessBirths10(.25f);
    check(engine.spawnVisible10_==4);check(engine.births10_.empty());
    auto count=engine.spawned_.size();engine.QueueActorSpawn(spawn);engine.CancelActorSpawns();check(engine.spawned_.size()==count);
    // Exact alias readback and parked release; no incomplete runtime package allocation.
    check(guard.packagePuts==0);
    auto first=engine.followers_.begin()->first;auto* recruited=engine.Ref(first)->As<RE::Actor>();
    auto slot=engine.followers_.at(first).aliasSlot;check(slot>=0);check(quest.GetAliasedRef(static_cast<std::uint32_t>(slot)).p==recruited);
    engine.Dismiss(first);check(quest.GetAliasedRef(static_cast<std::uint32_t>(slot)).p==&parking);check(recruited->factions[&members]==-1);
    quest.rejectBinding=true;RE::Actor rejected(701);rejected.base=&base;rejected.cell=&cell;rejected.runtime.currentProcess=&rejected.process;
    engine.Follow(&rejected);check(engine.followers_.at(rejected.id).aliasSlot==-1);check(!rejected.currentPackage);quest.rejectBinding=false;
    engine.Dismiss(rejected.id);
    // Missing authored runtime prevents registration and friend spawns. Quest
    // startup alone is not a prerequisite for the actor-local fallback.
    data.keys.erase({"FreedomControlRuntime.esp",fc::runtime10::Fallback});engine.Follow(&rejected);check(!engine.followers_.contains(rejected.id));
    engine.QueueActorSpawn(spawn);check(engine.spawnJobs_.empty());bind(fc::runtime10::Fallback,&follow);
    // Model-ready count does not rise for a half-created/disabled instance.
    spawn.count=1;spawn.recruit=false;engine.QueueActorSpawn(spawn);engine.ProcessActorSpawns();
    auto pendingID=engine.births10_.front().id;auto* pending=engine.Ref(pendingID)->As<RE::Actor>();pending->threeD=false;
    engine.ProcessBirths10(1);check(engine.spawnVisible10_==0);check(engine.births10_.size()==1);
    engine.ProcessBirths10(15);check(engine.spawnFailed_==1);check(engine.spawnVisible10_==0);
    // Paired door travel copies destination transform, not the door's mesh origin.
    RE::TESObjectREFR door(801),exit(802);door.cell=&cell;exit.cell=&other;door.base=exit.base=&base;
    RE::DoorTeleportData td{{&exit},{12,34,56},{0,0,1}};RE::ExtraTeleport xt{&td};door.extraList.extras[typeid(RE::ExtraTeleport)]=&xt;
    engine.DoorTransit10(&door);check(player.cell==&other);check(player.pos.x==12 && player.pos.z==56);
    player.cell=&cell;player.pos={0,0,0};
    // Pulse protects player and army, operates AFTER releasing the form map lock.
    engine.clearRadius10_=100000;engine.protectArmy10_=true;
    auto army=engine.followers_.begin()->first;engine.QueueClear10(false);
    while(!engine.clearJobs10_.empty())engine.ProcessClear10();
    check(!player.deleted);check(!engine.Ref(army)->IsDeleted());check(pending->deleted);
    // Native spell event publishes intent only; other caster/spell is ignored.
    auto events=engine.queue_.size();engine.OnSpellCast10(0x14,power.id);check(engine.queue_.size()==events+1);
    engine.OnSpellCast10(0x15,power.id);check(engine.queue_.size()==events+1);
    // Birth registration protects not-yet-bound creatures from our own aura.
    RE::Actor waiting(901);waiting.base=&base;waiting.cell=&cell;
    engine.births10_.push_back({waiting.id,base.id,true,false,false,{},0,engine.epoch_.load()});check(engine.IsLegionFriendly(&waiting));
    engine.births10_.clear();check(!engine.IsLegionFriendly(&waiting));
    // Generated enemies get only our enemy faction and a real combat request.
    engine.ConfigureEnemy10(&waiting);check(engine.enemies10_.contains(waiting.id));check(waiting.factions[&enemies]==0);check(waiting.combat);
    // Ongoing empty-world handling targets later loaded references and preserves the player.
    engine.emptyWorld10_=true;engine.ApplyFreedom10(fc::Action{.op=fc::Op::LoadedActor10,.target=waiting.id});check(!engine.clearJobs10_.empty());
    engine.ProcessClear10();check(waiting.deleted);check(!player.deleted);
    engine.ApplyFreedom10(fc::Action{.op=fc::Op::ClearCancel10});check(!engine.emptyWorld10_ && engine.clearJobs10_.empty());
    // A silent creature whose alias loses to another package gets only our fully
    // authored, non-created temporary package after bounded retries.
    RE::TESRace silentRace(1001);silentRace.data.flags.set(RE::RACE_DATA::Flag::kWalks);
    RE::Actor silent(1002);silent.base=&base;silent.cell=&cell;silent.race=&silentRace;
    silent.runtime.currentProcess=&silent.process;silent.rejectAliasPackage=true;
    engine.Follow(&silent);check(silent.packagePuts==0);check(!silent.currentPackage);
    for(int i=0;i<9;++i)engine.MaintainLegion(.25f,{});
    check(silent.packagePuts==1);check(silent.currentPackage==&follow);
    check(silent.lastTempPackage && !silent.lastCreatedPackage && !silent.lastAllowFurniture);
    // Late OnInit/OnLoad faction changes must not escape isolation or overwrite
    // their first restoration value.
    RE::TESFaction delayedFaction(1003);silent.factions[&delayedFaction]=7;
    for(int i=0;i<9;++i)engine.MaintainLegion(.25f,{});
    check(silent.factions[&delayedFaction]==-1);
    silent.factions[&delayedFaction]=4;
    for(int i=0;i<9;++i)engine.MaintainLegion(.25f,{});
    check(silent.factions[&delayedFaction]==-1);
    check(engine.followers_.at(silent.id).oldFactions.at(delayedFaction.id)==7);
    engine.Dismiss(silent.id);check(!silent.currentPackage);check(silent.factions[&delayedFaction]==7);
    check(silentRace.data.flags.any(RE::RACE_DATA::Flag::kWalks));
    // Same authored file does not imply that an arbitrary package is a follower.
    RE::TESPackage unrelated;unrelated.id=0xfe001900;unrelated.file=&ownFile;
    check(!fc::runtime10::IsOwnPackage(&unrelated));
    // Repeated native rejection is visible, throttled, and never faked as success.
    engine.Follow(&silent);silent.rejectPackage=true;silent.currentPackage=nullptr;
    auto packagePuts=silent.packagePuts;
    for(int i=0;i<60;++i)engine.MaintainLegion(.25f,{});
    check(!engine.followers_.at(silent.id).package);check(silent.packagePuts-packagePuts<=8);
    // Waiting/freeze and real combat always win over forced package recovery.
    auto& silentState=engine.followers_.at(silent.id);silentState.waiting=true;silent.ai=false;
    packagePuts=silent.packagePuts;silentState.retrySeconds=0;engine.SetFollowPackage(&silent,silentState);
    check(silent.packagePuts==packagePuts && !silent.ai);
    silentState.waiting=false;engine.freeze_=true;engine.SetFollowPackage(&silent,silentState);
    check(silent.packagePuts==packagePuts);engine.freeze_=false;
    silent.combat=true;engine.SetFollowPackage(&silent,silentState);check(silent.packagePuts==packagePuts);
    silent.combat=false;engine.Dismiss(silent.id);
    // Aquatic actors are not pulled onto land or land-navmesh; flyers are not
    // grounded; ground actors wait for a player who is airborne.
    RE::TESRace waterRace(1004),flyRace(1005);
    waterRace.data.flags.set(RE::RACE_DATA::Flag::kSwims);flyRace.data.flags.set(RE::RACE_DATA::Flag::kFlies);
    silent.rejectPackage=false;silent.rejectAliasPackage=false;silent.race=&waterRace;silent.pos={0,12000,0};
    engine.Follow(&silent);moved=silent.moves;auto snaps=silent.navmeshMoves;
    for(int i=0;i<20;++i)engine.MaintainLegion(.25f,{});
    check(silent.moves==moved);check(silent.navmeshMoves==snaps);
    player.swimming=true;for(int i=0;i<16;++i)engine.MaintainLegion(.25f,{});
    check(silent.moves>moved);check(silent.navmeshMoves==snaps);player.swimming=false;
    silent.race=&flyRace;silent.pos={0,12000,0};for(int i=0;i<16;++i)engine.MaintainLegion(.25f,{});
    check(silent.navmeshMoves==snaps);
    silent.race=&silentRace;silent.pos={0,12000,0};player.midair=true;moved=silent.moves;
    for(int i=0;i<16;++i)engine.MaintainLegion(.25f,{});
    check(silent.moves==moved);
    player.midair=false;player.swimming=true;moved=silent.moves;
    for(int i=0;i<16;++i)engine.MaintainLegion(.25f,{});
    check(silent.moves==moved); // A walk-only creature is not recovered into deep water.
    player.swimming=false;engine.Dismiss(silent.id);
    // Model readiness is separate from follow readback. Failed follow verification
    // keeps the actor registered, and a later accepted package recovers normally.
    spawn.count=1;spawn.recruit=true;engine.QueueActorSpawn(spawn);engine.ProcessActorSpawns();
    auto brokenID=engine.births10_.front().id;auto* broken=engine.Ref(brokenID)->As<RE::Actor>();
    broken->rejectPackage=true;broken->currentPackage=nullptr;
    engine.ProcessBirths10(.25f);check(engine.spawnVisible10_==1);check(engine.spawnFollowReady14_==0);
    check(engine.births10_.size()==1);
    for(int i=0;i<61;++i){engine.MaintainLegion(.25f,{});engine.ProcessBirths10(.25f);}
    check(engine.births10_.empty());check(engine.spawnFollowReady14_==0 && engine.spawnFollowPending14_==1);
    check(engine.followers_.contains(brokenID));check(engine.spawnVisible10_==1 && engine.spawnFailed_==0);
    broken->rejectPackage=false;for(int i=0;i<9;++i)engine.MaintainLegion(.25f,{});
    check(broken->currentPackage==&follow);
    // Dismissal during loading must not secretly recruit the creature again.
    engine.QueueActorSpawn(spawn);engine.ProcessActorSpawns();
    auto dismissedID=engine.births10_.front().id;
    auto* dismissed=engine.Ref(dismissedID)->As<RE::Actor>();
    engine.births10_.front().applyProfile=true;engine.births10_.front().profile.health=2000;
    const auto dismissedHealth=dismissed->GetBaseActorValue(RE::ActorValue::kHealth);
    const auto dismissedSnaps=dismissed->navmeshMoves;
    engine.Dismiss(dismissedID);engine.ProcessBirths10(.25f);
    check(dismissed->GetBaseActorValue(RE::ActorValue::kHealth)==dismissedHealth);
    check(dismissed->navmeshMoves==dismissedSnaps);
    // Release also cancels a pending birth that never made it into followers.
    engine.births10_.push_back({dismissedID,base.id,true,true,false,{},0,engine.epoch_.load()});
    engine.Dismiss(dismissedID);engine.ProcessBirths10(.25f);
    check(!engine.followers_.contains(dismissedID));check(engine.births10_.empty());
    // Removal also cancels delayed birth side effects, without calling the
    // dismissal path that could reactivate an actor during native deletion.
    engine.QueueActorSpawn(spawn);engine.ProcessActorSpawns();
    auto removedBirthID=engine.births10_.front().id;
    auto* removedBirth=engine.Ref(removedBirthID)->As<RE::Actor>();
    engine.RemoveReference(removedBirth);
    check(engine.births10_.empty());check(!removedBirth->ai && removedBirth->deleted);
    // A model-ready notification must not move an actor already in combat.
    engine.QueueActorSpawn(spawn);engine.ProcessActorSpawns();
    auto combatBirthID=engine.births10_.front().id;
    auto* combatBirth=engine.Ref(combatBirthID)->As<RE::Actor>();
    combatBirth->combat=true;combatBirth->runtime.currentCombatTarget={&enemy};
    const auto combatBirthSnaps=combatBirth->navmeshMoves;
    engine.ProcessBirths10(.25f);
    check(combatBirth->combat);check(combatBirth->navmeshMoves==combatBirthSnaps);
    engine.Dismiss(combatBirthID);
    // A wait issued while the birth job is pending remains authoritative.
    engine.QueueActorSpawn(spawn);engine.ProcessActorSpawns();
    auto waitID=engine.births10_.front().id;auto* newborn=engine.Ref(waitID)->As<RE::Actor>();
    engine.followers_.at(waitID).waiting=true;newborn->ai=false;newborn->currentPackage=nullptr;
    engine.ProcessBirths10(.25f);check(!newborn->ai);check(engine.followers_.at(waitID).waiting);
    // Actual individual wait/resume handler: no one-frame AI escape while the
    // overlay pauses maintenance, no combat left active after waiting, and no
    // accidental reactivation of disabled/dead references.
    newborn->combat=true;newborn->runtime.currentCombatTarget={&enemy};
    engine.WaitFollower(waitID,true);check(!newborn->combat && !newborn->ai);
    check(newborn->GetBaseActorValue(RE::ActorValue::kWaitingForPlayer)==1);
    engine.freeze_=true;auto evaluations=newborn->evaluations;
    auto puts=newborn->packagePuts;
    engine.WaitFollower(waitID,false);
    check(!newborn->ai && !engine.followers_.at(waitID).waiting);
    check(newborn->evaluations==evaluations && newborn->packagePuts==puts);
    engine.ProcessBirths10(.25f);check(!newborn->ai);
    engine.freeze_=false;engine.WaitFollower(waitID,false);check(newborn->ai);
    check(newborn->GetBaseActorValue(RE::ActorValue::kWaitingForPlayer)==0);
    newborn->disabled=true;newborn->ai=false;engine.WaitFollower(waitID,false);check(!newborn->ai);
    newborn->disabled=false;newborn->dead=true;engine.WaitFollower(waitID,false);check(!newborn->ai);
    newborn->dead=false;
    engine.CancelActorSpawns();
    engine.freeze_=true;engine.QueueActorSpawn(spawn);engine.ProcessActorSpawns();
    auto frozenID=engine.births10_.front().id;auto* frozen=engine.Ref(frozenID)->As<RE::Actor>();
    check(!frozen->ai);engine.ProcessBirths10(.25f);check(!frozen->ai);
    check(engine.followers_.at(frozenID).oldAI);engine.CancelActorSpawns();engine.freeze_=false;
    // A release under global freeze must preserve the original AI setting for
    // eventual thaw without activating the actor immediately.
    engine.freeze_=true;engine.Dismiss(frozenID);
    check(!frozen->ai);check(engine.frozen_.at(frozenID));engine.freeze_=false;
    // Combat crossing a cell boundary is not mistaken for failed navigation.
    RE::Actor fighter(1100),remoteEnemy(1101);
    for(auto* a:{&fighter,&remoteEnemy}){a->base=&base;a->cell=&other;a->runtime.currentProcess=&a->process;}
    engine.Follow(&fighter);engine.autoFight_=true;
    fighter.combat=true;fighter.runtime.currentCombatTarget={&remoteEnemy};
    moved=fighter.moves;puts=fighter.packagePuts;stopped=fighter.combatStops;
    for(int i=0;i<16;++i)engine.MaintainLegion(.25f,{});
    check(fighter.combat);check(fighter.combatStops==stopped);
    check(fighter.moves==moved && fighter.packagePuts==puts);
    fighter.combat=false;engine.autoFight_=false;engine.Dismiss(fighter.id);
    // Turning both hard control and enhanced follow off leaves rejection to the
    // authored alias, without silently installing the stronger temporary override.
    engine.hardControl_=false;engine.forcedFollow_=false;
    silent.race=&silentRace;silent.pos=player.pos;silent.rejectAliasPackage=true;
    engine.Follow(&silent);puts=silent.packagePuts;
    for(int i=0;i<20;++i)engine.MaintainLegion(.25f,{});
    check(silent.packagePuts==puts);check(!engine.followers_.at(silent.id).package);
    engine.Dismiss(silent.id);engine.hardControl_=true;
    // Every existing distance band/lane, fallback, and dragon orbit uses the
    // expected authored form. No creature-type package is allocated or mutated.
    std::array<RE::TESPackage,24> distancePackages;
    for(std::size_t i=0;i<distancePackages.size();++i) {
        auto& package=distancePackages[i];package.id=0xfe001820+static_cast<fc::ID>(i);package.file=&ownFile;
        bind(0x820+static_cast<fc::ID>(i),&package);
        check(fc::runtime10::IsOwnPackage(&package));
    }
    silent.dragon=false;
    for(std::size_t b=0;b<fc::FollowBands10.size();++b)for(int lane=0;lane<3;++lane)
        check(fc::runtime10::FollowPackage(&silent,15+lane,fc::FollowBands10[b])==&distancePackages[b*3+lane]);
    data.keys.erase({"FreedomControlRuntime.esp",0x820});
    check(fc::runtime10::FollowPackage(&silent,0,96)==&follow);
    RE::TESPackage orbit;orbit.id=0xfe001809;orbit.file=&ownFile;bind(fc::runtime10::Orbit,&orbit);
    silent.dragon=true;check(fc::runtime10::FollowPackage(&silent,0,96)==&orbit);
    check(!fc::runtime10::CanSnapToGround(&silent));silent.dragon=false;
    // Use packages/conditions/alias ordering parsed from the actual shipped ESP.
    // This is deliberately a selection/eligibility model, NOT a pathfinder or
    // evidence that Skyrim accepted a procedure tree. Template schema is audited
    // separately against primary xEdit/CK data.
    std::map<unsigned,RE::TESPackage> parsedPackages;
    std::map<RE::TESPackage*,const fixture15::Definition*> definitions;
    for(const auto& def:fixture15::definitions) {
        auto& package=parsedPackages[def.local];package.id=0xfe001000+def.local;package.file=&ownFile;
        bind(def.local,&package);definitions[&package]=&def;
        check(def.schemaReady && def.target==0x14);
    }
    auto eligible=[&](RE::Actor& actor,RE::TESPackage* package) {
        const auto found=definitions.find(package);if(found==definitions.end())return false;
        const auto& def=*found->second;
        if(!def.schemaReady || def.target!=0x14 || (def.program!=0x19b2c && def.program!=0x15b84))return false;
        for(const auto& condition:def.conditions) {
            if(condition.runOn!=0 || condition.op!=0)return false; // exact subset this ESP authors
            float observed=0;
            if(condition.function==74 && condition.parameter==0x804)observed=band.value;
            else if(condition.function==73 && condition.parameter==0x801) {
                auto found=actor.factions.find(&members);observed=found==actor.factions.end()?-1.0f:static_cast<float>(found->second);
            } else if(condition.function==71 && condition.parameter==0x803) {
                auto found=actor.factions.find(&dragons);observed=found!=actor.factions.end() && found->second>=0?1.0f:0.0f;
            } else return false;
            if(observed!=condition.comparison)return false;
        }
        return true;
    };
    RE::Actor deferred(1200),unbound(1201),noQuest(1202);
    for(auto* a:{&deferred,&unbound,&noQuest}) {
        a->base=&base;a->cell=&cell;a->pos=player.pos;a->runtime.currentProcess=&a->process;
        a->packageEligible15=[&,a](RE::TESPackage* p){return eligible(*a,p);};
        a->chooseAliasPackage15=[&,a]() -> RE::TESPackage* {
            if(!a->aliasPackage || !quest.IsRunning())return nullptr;
            for(auto local:fixture15::aliasOrder)if(auto* p=&parsedPackages.at(local);eligible(*a,p))return p;
            return nullptr;
        };
    }
    auto* authoredFallback=&parsedPackages.at(fc::runtime10::Fallback);
    quest.assignedPackage=authoredFallback;
    // Condition interpreter actually rejects inappropriate bands and dragon
    // packages; it is not the former unconditional always-accepting double.
    band.value=2;deferred.factions[&members]=1;
    check(eligible(deferred,&parsedPackages.at(0x827)));
    check(!eligible(deferred,&parsedPackages.at(0x826)));
    check(!eligible(deferred,&parsedPackages.at(0x829)));
    check(!eligible(deferred,&parsedPackages.at(0x809)));
    check(eligible(deferred,authoredFallback));
    deferred.PutCreatedPackage(&parsedPackages.at(0x826),true,false,false);
    check(!deferred.currentPackage);deferred.packagePuts=0;
    // First ordinary package selection is deferred until the engine advances.
    // No same-call mismatch is treated as success or immediately reset/forced.
    deferred.deferEvaluation15=true;engine.Follow(&deferred);
    check(!deferred.currentPackage && deferred.pendingPackage15);
    check(deferred.packagePuts==0 && deferred.resetEvaluations15==0);
    deferred.CompleteEvaluation15();check(fc::runtime10::IsOwnPackage(deferred.currentPackage));
    engine.followers_.at(deferred.id).retrySeconds=0;engine.SetFollowPackage(&deferred,engine.followers_.at(deferred.id));
    check(engine.followers_.at(deferred.id).status.find("别名跟随包已读回")!=std::string::npos);
    auto deferredInterrupts=deferred.interrupts15;
    for(int i=0;i<20;++i)engine.MaintainLegion(.25f,{});
    check(deferred.interrupts15==deferredInterrupts && deferred.packagePuts==0);
    engine.Dismiss(deferred.id);
    // A correctly authored fallback picked by the stack is legitimate even
    // when it differs from the predicted distance/lane package.
    engine.Follow(&deferred);deferred.CompleteEvaluation15();
    deferred.currentPackage=authoredFallback;deferred.pendingPackage15=nullptr;
    engine.followers_.at(deferred.id).retrySeconds=0;
    auto beforeEvaluations=deferred.evaluations;deferredInterrupts=deferred.interrupts15;
    for(int i=0;i<20;++i)engine.MaintainLegion(.25f,{});
    check(deferred.currentPackage==authoredFallback);
    check(deferred.evaluations==beforeEvaluations && deferred.interrupts15==deferredInterrupts);
    check(fc::runtime10::FollowTolerance(&deferred,3000,0)==432.0f);
    engine.Dismiss(deferred.id);
    // Alias rejection used to return before the supposed independent fallback.
    quest.rejectBinding=true;engine.Follow(&unbound);
    check(engine.followers_.contains(unbound.id) && engine.followers_.at(unbound.id).aliasSlot==-1);
    check(!unbound.currentPackage && unbound.packagePuts==0);
    for(int i=0;i<9;++i)engine.MaintainLegion(.25f,{});
    check(unbound.currentPackage==authoredFallback && unbound.packagePuts==1);
    check(engine.followers_.at(unbound.id).aliasSlot==-1);
    engine.followers_.at(unbound.id).retrySeconds=0;engine.SetFollowPackage(&unbound,engine.followers_.at(unbound.id));
    check(engine.followers_.at(unbound.id).status.find("保底跟随包已读回")!=std::string::npos);
    // When an alias first becomes available, promote once to the configured
    // band instead of trapping the actor in a fixed-distance temporary fallback.
    quest.rejectBinding=false;auto unboundInterrupts=unbound.interrupts15;
    for(int i=0;i<9;++i)engine.MaintainLegion(.25f,{});
    check(engine.followers_.at(unbound.id).aliasSlot>=0);
    check(unbound.currentPackage!=authoredFallback && fc::runtime10::IsOwnPackage(unbound.currentPackage));
    check(unbound.interrupts15==unboundInterrupts+1);
    for(int i=0;i<20;++i)engine.MaintainLegion(.25f,{});
    check(unbound.interrupts15==unboundInterrupts+1);
    engine.WaitFollower(unbound.id,true);puts=unbound.packagePuts;
    for(int i=0;i<12;++i)engine.MaintainLegion(.25f,{});
    check(!unbound.ai && unbound.packagePuts==puts);
    engine.freeze_=true;engine.WaitFollower(unbound.id,false);
    check(!unbound.ai && unbound.packagePuts==puts);engine.freeze_=false;
    engine.Dismiss(unbound.id);check(!unbound.currentPackage);
    // Async/rejected quest start no longer prevents existing actors or NEW
    // friendly spawns from using a reference-local authored fallback.
    quest.enabled=quest.running=false;quest.rejectStart=true;engine.questAttempt10_={};
    engine.Follow(&noQuest);check(engine.followers_.contains(noQuest.id));
    for(int i=0;i<9;++i)engine.MaintainLegion(.25f,{});
    check(noQuest.currentPackage==authoredFallback && noQuest.packagePuts==1);
    engine.followers_.at(noQuest.id).retrySeconds=0;engine.SetFollowPackage(&noQuest,engine.followers_.at(noQuest.id));
    check(engine.followers_.at(noQuest.id).status.find("保底跟随包已读回")!=std::string::npos);
    spawn.count=1;spawn.recruit=true;engine.QueueActorSpawn(spawn);check(!engine.spawnJobs_.empty());
    engine.ProcessActorSpawns();check(engine.spawnCreated_==1);
    auto fallbackBirthID=engine.births10_.front().id;
    auto* fallbackBirth=engine.Ref(fallbackBirthID)->As<RE::Actor>();
    fallbackBirth->packageEligible15=[&](RE::TESPackage* p){return eligible(*fallbackBirth,p);};
    for(int i=0;i<10;++i){engine.MaintainLegion(.25f,{});engine.ProcessBirths10(.25f);}
    check(fallbackBirth->currentPackage==authoredFallback);
    check(engine.spawnVisible10_==1 && engine.spawnFollowReady14_==1 && engine.births10_.empty());
    engine.Dismiss(fallbackBirthID);engine.Dismiss(noQuest.id);
    // Dragon emergency path is honestly the generic unconditional follow, not
    // a claim that the native engine switched it to a flying orbit. When the
    // alias is recovered it promotes exactly once to the authored orbit.
    noQuest.dragon=true;engine.dragonsIndoors_=true;engine.Follow(&noQuest);
    for(int i=0;i<9;++i)engine.MaintainLegion(.25f,{});
    check(noQuest.currentPackage==authoredFallback);
    check(fc::runtime10::FollowTolerance(&noQuest,240,0)==432.0f);
    check(!fc::runtime10::CanSnapToGround(&noQuest));
    quest.rejectStart=false;quest.enabled=quest.running=true;
    noQuest.combat=true;noQuest.runtime.currentCombatTarget={&enemy};
    engine.followers_.at(noQuest.id).retrySeconds=0;
    engine.SetFollowPackage(&noQuest,engine.followers_.at(noQuest.id));
    check(engine.followers_.at(noQuest.id).aliasRefreshPending15);
    check(noQuest.currentPackage==authoredFallback);
    engine.freeze_=true;engine.SetFollowPackage(&noQuest,engine.followers_.at(noQuest.id));
    check(engine.followers_.at(noQuest.id).aliasRefreshPending15);engine.freeze_=false;
    engine.followers_.at(noQuest.id).waiting=true;
    engine.SetFollowPackage(&noQuest,engine.followers_.at(noQuest.id));
    check(engine.followers_.at(noQuest.id).aliasRefreshPending15);
    engine.followers_.at(noQuest.id).waiting=false;
    noQuest.combat=false;engine.SetFollowPackage(&noQuest,engine.followers_.at(noQuest.id));
    check(!engine.followers_.at(noQuest.id).aliasRefreshPending15);
    check(noQuest.currentPackage==&parsedPackages.at(fc::runtime10::Orbit));
    check(fc::runtime10::FollowTolerance(&noQuest,240,0)==4200.0f);
    engine.Dismiss(noQuest.id);engine.dragonsIndoors_=false;
    std::cout<<"PASS: "<<checks<<" actual Legion/Follower10/Spawner10/WorldFreedom10 control-flow assertions against explicit test doubles. NOT CommonLib ABI/gameplay validation.\n";
    // Engine owns real game references; this harness owns fake allocations and cleans them.
    RE::allocated.clear();
}
