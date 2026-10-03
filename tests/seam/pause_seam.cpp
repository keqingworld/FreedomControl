#include "PCH.h"
#include "Engine.hpp"
#include <iostream>
#include <source_location>
namespace fc {
std::vector<Op> applied;
unsigned batches{},views{},hud{},flightTicks{},maintains{},cancels{};
bool throwApply{};
float birthDt{},removalDt{};
namespace law11 {void SetPolicy(bool,bool){}bool Install(){return true;}}
void Engine::Apply(const Action& a){
    if(!SKSE::inGameTask)throw std::logic_error("Gameplay action escaped game task");
    if(throwApply){throwApply=false;throw std::runtime_error("test action failure");}
    if(a.afterClose||a.op==Op::Goto||a.op==Op::Rest11) {
        if(NativePauseOpen15()||OtherNativePause15()||inputBlocked_)throw std::logic_error("Deferred action ran before native close readback");
    }
    applied.push_back(a.op);
}
RE::TESObjectREFR* Engine::Ref(ID id)const{return RE::TESForm::LookupByID<RE::TESObjectREFR>(id);}
void Engine::Note(std::string s){notes_.push_back(std::move(s));}
void Engine::RefreshHUD(){if(SKSE::inUITask)throw std::logic_error("HUD in pause pump");++hud;}
void Engine::RefreshView(){++views;}
void Engine::RefreshInspection(){}
void Engine::TickKernel11(float){}
int Engine::AddForm(RE::TESForm*,int,bool,RE::Actor*){if(!SKSE::inGameTask)throw std::logic_error("Batch escaped game task");++batches;return 1;}
void Engine::ProcessActorSpawns(){}
void Engine::ProcessBirths10(float dt){birthDt=dt;}
void Engine::ProcessClear10(){}
void Engine::MaintainRemovals(float dt){removalDt=dt;}
void Engine::RefillResources(){}
void Engine::MaintainQuests11(float){}
void Engine::MaintainFlight(float){++flightTicks;}
void Engine::Maintain(float){++maintains;}
void Engine::CancelActorSpawns(){++cancels;spawnJobs_.clear();}
void Engine::RestoreQuestTexts11(){}
void Engine::Run(std::string_view,RE::TESObjectREFR*){}
void Engine::BuildCatalog(){}
void Engine::ResetSession(){ready_=false;queue_.clear();afterClose_.clear();}
bool Engine::EnsureRuntime10(){return true;}
void Engine::ConfigureClaim(const ClaimState&){}
}
int main(){
    using namespace fc;unsigned checks=0;
    auto ck=[&](bool b,const std::source_location loc=std::source_location::current()){
        ++checks;if(!b)throw std::runtime_error("Pause seam #"+std::to_string(checks)+" line "+std::to_string(loc.line()));
    };
    RE::TESObjectCELL cell(100);RE::PlayerCharacter player(0x14);player.cell=&cell;RE::PlayerCharacter::instance=&player;
    auto* ui=RE::UI::GetSingleton();auto* native=RE::UIMessageQueue::GetSingleton();auto* tasks=SKSE::GetTaskInterface();
    auto* main=RE::Main::GetSingleton();auto* controls=RE::ControlMap::GetSingleton();
    auto reset=[&] {
        ui->menus.clear();ui->numPausesGame=0;native->queued.clear();tasks->game.clear();tasks->ui.clear();
        main->data.freezeTime=false;controls->enabled=0x5a5;controls->stored=0xa55a;
        player.playerFlags.isLoading=false;player.cell=&cell;applied.clear();batches=views=hud=flightTicks=maintains=cancels=0;
        throwApply=false;birthDt=removalDt=-1;RE::Main::available=RE::UI::available=RE::UIMessageQueue::available=SKSE::tasksAvailable=true;
    };
    auto frame=[&](Engine& e,bool game=true){e.RequestTick();tasks->UIFrame();if(game)tasks->GameFrame();};
    auto open=[&](Engine& e){e.ready_=true;e.SetMenuOpen(true);frame(e);frame(e);ck(e.NativePauseOpen15()&&ui->numPausesGame==1);};
    reset();{
        // Assertion #1 is also the immutable-baseline negative-control sentinel:
        // the old exact SyncInput leaves the world unfrozen when pauseMenu=false.
        Engine e;e.ready_=true;e.menuOpen_=true;e.pauseMenu_=false;e.SyncInput();
        ck(main->data.freezeTime&&e.pauseLease_.Held()&&e.pauseMenu_&&e.inputBlocked_);
        e.menuOpen_=false;e.SyncInput();ck(!main->data.freezeTime&&controls->enabled==0x5a5);
    }
    reset();{
        Engine e;e.pauseMenu_=false;open(e);ck(e.pauseMenu_&&e.NativePauseConfirmed15()&&!main->data.freezeTime&&!e.pauseLease_.Held());
        ck(controls->enabled==0&&controls->stored==0xa55a);ck(!e.OtherNativePause15());
        ck(applied.size()==1&&applied[0]==Op::SelectCrosshair);ck(birthDt==0&&removalDt==0&&flightTicks==0);
        e.Submit(Action{.op=Op::SetValue});frame(e);ck(applied.back()==Op::SetValue&&e.MenuOpen());
        e.batchPerTick_=2;for(int i=0;i<5;++i)e.batch_.push_back(Action{.op=Op::Add,.count=1,.epoch=e.epoch_});
        e.SetMenuOpen(false);frame(e);ck(e.MenuOpen()&&batches==2&&ui->numPausesGame==1);
        frame(e);ck(e.MenuOpen()&&batches==4);frame(e);ck(!e.MenuOpen()&&batches==5);
        ck(!main->data.freezeTime&&controls->enabled==0x5a5&&controls->stored==0xa55a);
        frame(e);frame(e);ck(!e.NativePauseOpen15()&&ui->numPausesGame==0&&!e.pauseLease_.Held()&&!e.closeFinalization15_);
        ck(hud>0);
    }
    reset();{
        Engine e;open(e);e.Submit(Action{.op=Op::Goto});frame(e);
        ck(!e.MenuOpen()&&e.NativePauseOpen15()&&e.afterClose_.size()==1);
        ck(std::find(applied.begin(),applied.end(),Op::Goto)==applied.end());
        frame(e);frame(e);ck(applied.back()==Op::Goto&&e.afterClose_.empty()&&ui->numPausesGame==0);
    }
    reset();{
        Engine e;open(e);e.Submit(Action{.op=Op::Command,.afterClose=true});frame(e);
        ++ui->numPausesGame; // A different native menu opens while our hide is pending.
        frame(e);frame(e);ck(ui->numPausesGame==1&&!e.NativePauseOpen15()&&e.afterClose_.size()==1);
        --ui->numPausesGame;frame(e);ck(e.afterClose_.empty()&&applied.back()==Op::Command);
    }
    reset();{
        Engine e;ui->numPausesGame=1;main->data.freezeTime=true;e.ready_=true;e.SetMenuOpen(true);
        frame(e);frame(e);ck(ui->numPausesGame==2&&e.OtherNativePause15());
        // External pause does not make F8's close unavailable. Pending editor
        // actions are preserved and may resume after the external menu ends.
        e.SetMenuOpen(false);frame(e);frame(e);frame(e);
        ck(ui->numPausesGame==1&&!e.NativePauseOpen15()&&main->data.freezeTime);
        ck(controls->enabled==0x5a5&&!e.inputBlocked_&&!e.pauseLease_.Held());
        --ui->numPausesGame;frame(e);frame(e);ck(applied.size()==1);
    }
    reset();{
        Engine e;open(e);++ui->numPausesGame;frame(e);--ui->numPausesGame;frame(e);
        ck(e.NativePauseOpen15()&&ui->numPausesGame==1);e.SetMenuOpen(false);frame(e);frame(e);frame(e);ck(ui->numPausesGame==0);
    }
    reset();{
        Engine e;e.ready_=true;e.menuOpen_=true;e.SyncInput();e.SyncNativePause15();
        e.menuOpen_=false;e.SyncInput();e.SyncNativePause15();
        ck(native->queued.size()==2);native->Drain();ck(ui->numPausesGame==0&&!e.NativePauseOpen15());
        e.nativePauseRequest15_.reset();e.SyncNativePause15();ck(native->queued.empty());
        // Repeated close/hide cannot remove another menu's contribution.
        ui->numPausesGame=2;for(int i=0;i<10;++i){e.SetMenuOpen(false);frame(e);}ck(ui->numPausesGame==2);
    }
    reset();{
        Engine e;open(e);e.SetMenuOpen(false); // Explicit close / emergency overlay failure path.
        frame(e,false);ck(!e.MenuOpen()&&!main->data.freezeTime&&controls->enabled==0x5a5);
        ck(tasks->game.size()==1);frame(e,false);frame(e,false);ck(!e.NativePauseOpen15()&&ui->numPausesGame==0&&tasks->game.size()==1);
        tasks->GameFrame();ck(!e.tickPending_&&!e.pauseTickPending15_);
    }
    reset();{
        Engine e;open(e);e.Submit(Action{.op=Op::SetValue});throwApply=true;frame(e);
        ck(!e.MenuOpen()&&!main->data.freezeTime&&!e.inputBlocked_&&e.queue_.empty()&&!e.notes_.empty());
        frame(e);frame(e);ck(!e.NativePauseOpen15()&&ui->numPausesGame==0);
    }
    reset();{
        Engine e;open(e);e.Submit(Action{.op=Op::Goto});e.afterClose_.push_back(Action{.op=Op::Rest11});
        auto epoch=e.epoch_.load();SKSE::MessagingInterface::Message load{SKSE::MessagingInterface::kPreLoadGame,nullptr};
        e.Message(&load);ck(!e.ready_&&!e.MenuOpen()&&e.epoch_==epoch+1&&e.queue_.empty()&&e.afterClose_.empty());
        ck(!main->data.freezeTime&&controls->enabled==0x5a5);frame(e);frame(e);ck(!e.NativePauseOpen15()&&ui->numPausesGame==0);
        load={SKSE::MessagingInterface::kPostLoadGame,nullptr};e.Message(&load);ck(!e.ready_);
    }
    reset();{
        Engine e;open(e);player.playerFlags.isLoading=true;frame(e);frame(e);ck(!e.MenuOpen()&&ui->numPausesGame==0&&controls->enabled==0x5a5);
    }
    reset();{
        Engine e;open(e);native->AddMessage("FreedomControlPause15",RE::UI_MESSAGE_TYPE::kForceHide,nullptr);
        frame(e);ck(!e.NativePauseOpen15()&&main->data.freezeTime);frame(e);ck(e.NativePauseOpen15()&&ui->numPausesGame==1);
        e.SetMenuOpen(false);frame(e);frame(e);frame(e);ck(ui->numPausesGame==0);
    }
    reset();{
        Engine e;open(e);e.SetMenuOpen(false);RE::UIMessageQueue::available=false;frame(e);ck(e.NativePauseOpen15()&&!e.inputBlocked_);
        RE::UIMessageQueue::available=true;frame(e);frame(e);ck(ui->numPausesGame==0);
    }
    reset();{
        Engine e;open(e);RE::Main::available=false;e.SetMenuOpen(false);frame(e);frame(e);ck(!e.pauseLease_.Held()&&!e.inputBlocked_);
        RE::Main::available=true;frame(e);ck(!e.pauseLease_.Held()&&!main->data.freezeTime&&ui->numPausesGame==0);
    }
    reset();{
        Engine e;e.ready_=true;for(int cycle=0;cycle<100;++cycle){
            const bool paused=cycle%2;main->data.freezeTime=paused;controls->enabled=static_cast<std::uint32_t>(cycle+4);controls->stored=static_cast<std::uint32_t>(cycle+40);
            open(e);e.SetMenuOpen(false);frame(e);frame(e);frame(e);
            ck(ui->numPausesGame==0&&main->data.freezeTime==paused&&controls->enabled==static_cast<std::uint32_t>(cycle+4)&&controls->stored==static_cast<std::uint32_t>(cycle+40));
        }
    }
    reset();{
        Engine e;open(e);ck(!e.pauseLease_.Held());
        main->data.freezeTime=true; // Another owner pauses after our transition lease is released.
        e.SetMenuOpen(false);frame(e);frame(e);frame(e);ck(ui->numPausesGame==0&&main->data.freezeTime);
    }
    reset();{
        Engine e;e.ready_=true;e.SetMenuOpen(true);frame(e);ck(main->data.freezeTime&&e.pauseLease_.Held());
        RE::Main::available=false;e.SetMenuOpen(false);frame(e);frame(e);ck(e.pauseLease_.Held());
        RE::Main::available=true;frame(e);ck(!e.pauseLease_.Held()&&!main->data.freezeTime&&ui->numPausesGame==0);
    }
    reset();{
        Engine e;open(e);e.SetMenuOpen(false);frame(e);
        e.SetMenuOpen(true);frame(e);frame(e);ck(e.MenuOpen()&&ui->numPausesGame==1&&controls->enabled==0);
        e.SetMenuOpen(false);frame(e);frame(e);frame(e);ck(ui->numPausesGame==0&&controls->enabled==0x5a5);
    }
    reset();{
        Engine e;e.ready_=true;e.SetMenuOpen(true);frame(e);ck(!e.NativePauseOpen15()&&!native->queued.empty());
        SKSE::MessagingInterface::Message load{SKSE::MessagingInterface::kPreLoadGame,nullptr};e.Message(&load);
        frame(e);frame(e);frame(e);ck(!e.NativePauseOpen15()&&ui->numPausesGame==0&&!e.pauseLease_.Held()&&controls->enabled==0x5a5);
    }
    reset();{
        Engine e;e.ready_=true;e.SetMenuOpen(true);
        RE::UIMessageQueue::available=false;e.RequestTick();
        ck(!e.pauseTickPending15_&&tasks->ui.empty());tasks->GameFrame();
        RE::UIMessageQueue::available=true;frame(e);frame(e);ck(e.NativePauseConfirmed15());
        e.SetMenuOpen(false);frame(e);frame(e);frame(e);ck(ui->numPausesGame==0);
    }
    reset();{
        Engine e;e.ready_=true;e.SetMenuOpen(true);RE::UI::available=false;
        e.RequestTick();ck(!e.pauseTickPending15_&&tasks->ui.empty());tasks->GameFrame();
        RE::UI::available=true;frame(e);frame(e);ck(e.NativePauseConfirmed15());
        e.SetMenuOpen(false);frame(e);frame(e);frame(e);ck(ui->numPausesGame==0);
    }
    reset();{
        Engine e;e.ready_=true;e.SetMenuOpen(true);e.RequestTick();
        const auto stale=e.pauseTickPending15_.load();ck(stale!=0&&tasks->ui.size()==1);
        SKSE::MessagingInterface::Message load{SKSE::MessagingInterface::kPreLoadGame,nullptr};e.Message(&load);
        ck(!e.pauseTickPending15_);e.RequestTick();const auto current=e.pauseTickPending15_.load();
        ck(current!=0&&current!=stale&&tasks->ui.size()==2);
        // Run the stale callback alone: it must not release the current latch or
        // create a show/hide request for the old menu session.
        auto old=std::move(tasks->ui.front());tasks->ui.pop_front();old();
        ck(e.pauseTickPending15_==current&&native->queued.empty());
        tasks->UIFrame();tasks->GameFrame();ck(!e.pauseTickPending15_&&!e.MenuOpen()&&ui->numPausesGame==0);
    }
    reset();{
        Engine e;e.ready_=true;e.SetMenuOpen(true);e.RequestTick();ck(e.pauseTickPending15_!=0);
        RE::UIMessageQueue::available=false;tasks->ui.clear(); // UI shutdown discards its queued callback.
        e.RequestTick();ck(!e.pauseTickPending15_);tasks->GameFrame();
        RE::UIMessageQueue::available=true;frame(e);frame(e);ck(e.NativePauseConfirmed15());
        e.SetMenuOpen(false);frame(e);frame(e);frame(e);ck(ui->numPausesGame==0);
    }
    reset();{
        auto& e=Engine::Get();open(e);
        fakeForeground=reinterpret_cast<HWND>(1);fakeKeyDown=false;overlay::PollMenuHotkey();
        fakeForeground=reinterpret_cast<HWND>(2);fakeKeyDown=true;overlay::PollMenuHotkey();frame(e);
        ck(e.MenuOpen()&&e.NativePauseConfirmed15()&&!main->data.freezeTime&&controls->enabled==0);
        // Returning with F8 held primes the hotkey without closing the panel.
        fakeForeground=reinterpret_cast<HWND>(1);overlay::PollMenuHotkey();frame(e);
        ck(e.MenuOpen()&&ui->numPausesGame==1);
        fakeKeyDown=false;overlay::PollMenuHotkey();fakeKeyDown=true;overlay::PollMenuHotkey();
        frame(e);frame(e);frame(e);ck(!e.MenuOpen()&&!e.NativePauseOpen15()&&!main->data.freezeTime&&controls->enabled==0x5a5);
    }
    std::cout<<"PASS: "<<checks<<" production pause lifecycle/Tick assertions with queued native menu and separate UI/game task doubles. NOT a Windows ABI or Skyrim behavior test.\n";
}
