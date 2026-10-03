#include "PCH.h"
#include "Engine.hpp"
#include <RE/B/BGSScene.h>
#include <RE/M/MenuTopicManager.h>
#include <RE/U/UIMessageQueue.h>

namespace fc {
namespace {
double IntentTime14() {
    return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}
}
void Engine::RecordPlayerActivation14() {
    // Called by Skyrim's mapped input event sink. No engine mutation, no physical
    // E-key assumption, no synthetic TESActivateEvent treated as player consent.
    std::scoped_lock lock(stateMutex_);
    const auto generation=dialogueGeneration14_.load(std::memory_order_relaxed);
    if(generation!=observedDialogueGeneration14_) {playerIntent14_.Reset();observedDialogueGeneration14_=generation;}
    // A new Activate attempt at an invalid/non-actor target revokes the old
    // pending target rather than leaving it available for an unrelated greeting.
    playerIntent14_.CancelPending();
    auto* p=RE::PlayerCharacter::GetSingleton();auto* ui=RE::UI::GetSingleton();
    if(!ready_ || !rules11_.playerFreedom14 || !rules11_.preserveVoluntary14 ||
       menuOpen_.load() || inputBlocked_ || std::chrono::steady_clock::now()<suspendUntil11_ || !p || p->IsDead(false) ||
       p->GetPlayerFlags().isLoading || !ui || ui->GameIsPaused() ||
       ui->IsItemMenuOpen() || ui->IsModalMenuOpen() || ui->IsApplicationMenuOpen() ||
       ui->IsMenuOpen(RE::BSFixedString("Dialogue Menu")))return;
    if(auto* controls=RE::ControlMap::GetSingleton()) {
        std::uint32_t enabled{},stored{};controls->GetControlsState(enabled,stored);
        if(!(enabled & (1u<<2)))return;
    } else return;
    auto* pick=RE::CrosshairPickData::GetSingleton();if(!pick)return;
    auto target=pick->GetActiveTarget().get();auto* actor=target?target->As<RE::Actor>():nullptr;
    if(!actor || actor==p || actor->IsDead(false) || actor->IsDisabled() || actor->IsDeleted())return;
    // Engine activation reach is modifiable; do not hardcode vanilla distance.
    // The current actor handle is the narrowest input-supported identity.
    playerIntent14_.Activate(actor->GetFormID(),IntentTime14());
}

bool Engine::TickPlayerFreedom14(bool dialogue) {
    const auto generation=dialogueGeneration14_.load(std::memory_order_relaxed);
    if(generation!=observedDialogueGeneration14_) {playerIntent14_.Reset();observedDialogueGeneration14_=generation;}
    auto* p=RE::PlayerCharacter::GetSingleton();if(!p){playerIntent14_.Reset();return dialogue;}
    if(!rules11_.playerFreedom14) {playerIntent14_.Reset();return dialogue;}
    auto* topic=RE::MenuTopicManager::GetSingleton();
    auto speaker=topic?topic->speaker.get():RE::NiPointer<RE::TESObjectREFR>{};
    auto* actor=speaker?speaker->As<RE::Actor>():nullptr;
    const ID speakerID=actor?actor->GetFormID():0;
    auto* scene=p->GetCurrentScene();const ID sceneID=scene?scene->GetFormID():0;
    const bool forcedGreeting=actor && actor->GetActorRuntimeData().boolBits.any(RE::Actor::BOOL_BITS::kForceGreetingPlayer);
    // Some native force-greet paths publish the speaker before opening the UI.
    // Cancel that narrow pending interaction too; ordinary ambient hellos survive.
    const auto decision=playerIntent14_.Observe(dialogue || forcedGreeting,speakerID,sceneID,IntentTime14(),true,rules11_.preserveVoluntary14);
    const auto now=std::chrono::steady_clock::now();
    // Readback/retry rather than assuming one asynchronous Hide message succeeded.
    // Rate-limited retries keep hostile mod scripts from flooding the UI queue/log.
    if((decision.closeDialogue || decision.releaseScene) && now>=freedomActionAt14_) {
        freedomActionAt14_=now+std::chrono::milliseconds(150);
        if(decision.closeDialogue) {
            if(actor && !actor->IsDeleted() && !actor->IsDead(false)) {
                actor->StopCurrentDialogue();
                auto& data=actor->GetActorRuntimeData();
                if(data.boolBits.any(RE::Actor::BOOL_BITS::kForceGreetingPlayer)) {
                    data.boolBits.reset(RE::Actor::BOOL_BITS::kForceGreetingPlayer);
                    if(data.currentProcess)actor->EndInterruptPackage(false);
                }
            }
            p->StopCurrentDialogue();
            if(auto* queue=RE::UIMessageQueue::GetSingleton())
                queue->AddMessage(RE::BSFixedString("Dialogue Menu"),RE::UI_MESSAGE_TYPE::kHide,nullptr);
            ++rejectedDialogues14_;
            if(rejectedDialogues14_<=10 || rejectedDialogues14_%100==0)
                spdlog::info("Freedom14: rejected unsolicited dialogue, speaker={:08X}, playerScene={:08X}, attempt={}",speakerID,sceneID,rejectedDialogues14_);
        }
        if(decision.releaseScene) {
            // Detach only the player; never stop/reset/complete the owning quest,
            // edit scene arrays or disable another NPC's scripts/base records.
            if(!decision.voluntaryDialogue)p->StopCurrentDialogue();
            p->SetCurrentScene(nullptr);
            p->GetActorRuntimeData().boolFlags.reset(RE::Actor::BOOL_FLAGS::kScenePackage);
            if(p->GetActorRuntimeData().currentProcess)p->EndInterruptPackage(decision.voluntaryDialogue);
            ++releasedScenes14_;
            if(releasedScenes14_<=10 || releasedScenes14_%100==0)
                spdlog::info("Freedom14: released player from scene {:08X}, attempt={}",sceneID,releasedScenes14_);
        }
        dirty_=true;
    }
    // Preserve input isolation while a deliberate conversation (or its bounded
    // unknown-speaker opening grace) owns the UI. Unsolicited UI is being closed.
    return dialogue && !decision.closeDialogue;
}
}
