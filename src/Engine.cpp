#include "PCH.h"
#include "Engine.hpp"
#include "Runtime10.hpp"
#include "Overlay.hpp"
#include "Input13.hpp"
#include <RE/A/AIProcess.h>
#include <RE/B/BGSScene.h>
#include <RE/B/BGSDefaultObjectManager.h>
#include <RE/B/BGSSoundCategory.h>
#include <RE/T/TESLevCharacter.h>
#include <RE/A/ActorValueList.h>
#include <RE/H/HUDData.h>
#include <RE/H/HUDMenu.h>
#include <RE/M/Main.h>
#include <RE/I/IMenu.h>
#include <RE/U/UIMessageQueue.h>

namespace fc {
namespace {
using Clock = std::chrono::steady_clock;
constexpr std::uint32_t kSerialization = 0x46524331; // FRC1
constexpr std::uint32_t kStateRecord = 0x53544154;   // STAT
constexpr std::uint32_t kRecordVersion = 1;
constexpr std::uint32_t kMaxStateBytes = 32 * 1024 * 1024;
constexpr ID kPlayer = 0x14;
// This movie-less menu owns a normal engine pause through the menu stack.
// The engine adjusts numPausesGame and sends menu-mode events on show/hide.
// Do not write shared pause counters or use freeze-frame flags: ImGui/Present,
// the native UI queue, and SKSE UI tasks must continue to advance.
class PauseMenu15 final : public RE::IMenu {
public:
    inline static constexpr char MENU_NAME[] = "FreedomControlPause15";
    PauseMenu15() { menuFlags.set(RE::UI_MENU_FLAGS::kPausesGame); }
    static RE::IMenu* Create() { return new PauseMenu15; }
};
std::string Name(RE::TESForm* form) {
    if (!form) return "<none>";
    auto* reference = form->AsReference();
    const auto* name = reference ? reference->GetDisplayFullName() : form->GetName();
    if (name && *name) return name;
    const auto* editor = form->GetFormEditorID();
    return editor && *editor ? std::string(editor) : "[" + Hex(form->GetFormID()) + "]";
}
std::array<float, 3> Point(const RE::NiPoint3& p) { return {p.x, p.y, p.z}; }
RE::NiPoint3 Point(const std::array<float, 3>& p) { return {p[0], p[1], p[2]}; }
std::vector<ID> CellRefs(RE::TESObjectCELL* cell) {
    std::vector<ID> result;
    if (cell) cell->ForEachReference([&](RE::TESObjectREFR* ref) {
        if (ref) result.push_back(ref->GetFormID());
        return RE::BSContainer::ForEachResult::kContinue;
    });
    // Never change a cell's references while its enumeration lock is held.
    return result;
}
std::vector<ID> Actors() {
    std::unordered_set<ID> unique;
    if (auto* processes = RE::ProcessLists::GetSingleton()) {
        processes->ForAllActors([&](RE::Actor* actor) {
            if (actor && actor->GetFormID() != kPlayer) unique.insert(actor->GetFormID());
            return RE::BSContainer::ForEachResult::kContinue;
        });
    }
    if (auto* player = RE::PlayerCharacter::GetSingleton()) {
        for (auto id : CellRefs(player->GetParentCell())) {
            auto* form = RE::TESForm::LookupByID(id);
            if (form && form->As<RE::Actor>() && id != kPlayer) unique.insert(id);
        }
    }
    return {unique.begin(), unique.end()};
}
}

Engine& Engine::Get() { static Engine engine; return engine; }
void Engine::Initialize() {
    std::scoped_lock stateLock(stateMutex_);
    const auto ini = std::filesystem::absolute(L"Data/SKSE/Plugins/FreedomControl.ini");
    hotkey_ = static_cast<int>(GetPrivateProfileIntW(L"Input", L"MenuVirtualKey", VK_F8, ini.c_str()));
    hotkey_ = std::clamp(hotkey_, 1, 254);
    batchPerTick_ = std::clamp(static_cast<int>(GetPrivateProfileIntW(L"Performance", L"BatchPerTick", 12, ini.c_str())), 1, 64);
    crimeHooksAllowed11_=GetPrivateProfileIntW(L"Kernel",L"CrimeHooks",1,ini.c_str())!=0;
    packageFollow_ = GetPrivateProfileIntW(L"Followers", L"NativeFollowPackage", 1, ini.c_str()) != 0;
    if (auto* serial = SKSE::GetSerializationInterface()) {
        serial->SetUniqueID(kSerialization);
        serial->SetSaveCallback(Save);
        serial->SetLoadCallback(Load);
        serial->SetRevertCallback(Revert);
    }
    // Legacy INI/co-save opt-outs are ignored: F8 always pauses the world.
    pauseMenu_ = true;
    fontScale_ = Finite(static_cast<float>(GetPrivateProfileIntW(L"Interface", L"FontScalePercent", 100, ini.c_str()))/100.0f,1,0.8f,1.6f);
    forcedFollow_ = GetPrivateProfileIntW(L"Followers", L"ForcedTether", 0, ini.c_str()) != 0;
    autoFight_=GetPrivateProfileIntW(L"Legion",L"AutoFight",1,ini.c_str())!=0;
    hardControl_=GetPrivateProfileIntW(L"Legion",L"HardControl",1,ini.c_str())!=0;
    isolateFactions_=GetPrivateProfileIntW(L"Legion",L"IsolateFactions",1,ini.c_str())!=0;
    battleRadius_=Finite(static_cast<float>(GetPrivateProfileIntW(L"Legion",L"BattleRadius",5000,ini.c_str())),5000,256,20000);
    defaultAutoFight_=autoFight_; defaultHardControl_=hardControl_; defaultIsolateFactions_=isolateFactions_; defaultBattleRadius_=battleRadius_;
    Note("原生暂停与独立跟随修复15：F8 完整暂停世界，恢复原生跟随与战斗；原有任务中心、军团、自由内核和输入功能保留。");
}
void Engine::Message(SKSE::MessagingInterface::Message* m) {
    std::scoped_lock stateLock(stateMutex_);
    if (!m) return;
    using M = SKSE::MessagingInterface;
    switch (m->type) {
    case M::kDataLoaded:
        BuildCatalog();
        runtime10::InstallEvents();
        input13::InstallGameInput();
        if(crimeHooksAllowed11_)law11::Install();
        else Note("Kernel11: native crime hooks disabled by INI; periodic cleanup only.");
        if (!overlay::Install()) Note("覆盖层 Hook 失败，请查看 FreedomControl.log。");
        break;
    case M::kPreLoadGame:
        playerIntent14_.Reset();freedomActionAt14_={};
        ready_ = false;
        law11::SetPolicy(false,false);
        menuOpen_ = false;
        RestoreTransient();
        ++epoch_;
        { std::scoped_lock lock(queueMutex_); queue_.clear(); }
        batch_.clear();
        break;
    case M::kNewGame:
        RestoreTransient();
        ResetSession();
        ready_ = true;
        EnsureRuntime10();
        break;
    case M::kPostLoadGame:
        ready_ = m->data != nullptr;
        if (!ready_) Note("存档读取失败，操作仍保持禁用。");
        else { aliasReconciled10_=false; questDirty11_=true; syncedPersonal11_.fill(0); EnsureRuntime10(); for (const auto& [id,claim]:claims_) ConfigureClaim(claim); dirty_=true; }
        break;
    default: break;
    }
}
void Engine::Submit(Action a) {
    if (!a.epoch) a.epoch = epoch_.load();
    std::scoped_lock lock(queueMutex_);
    if (queue_.size() < 2048) queue_.push_back(std::move(a));
}
void Engine::SetMenuOpen(bool open) {
    // Render/input thread only publishes intent. Game/UI tasks own engine mutations.
    if (open) {
        closeRequested_ = false;
        if (!menuOpen_.exchange(true)) Submit(Action{.op = Op::SelectCrosshair});
    } else closeRequested_ = true;
}
View Engine::Snapshot() const { std::scoped_lock lock(viewMutex_); return view_; }
std::shared_ptr<const std::vector<Entry>> Engine::Catalog() const {
    std::scoped_lock lock(catalogMutex_); return catalog_;
}
void Engine::PumpPause15() {
    std::scoped_lock stateLock(stateMutex_);
    // SKSE AddUITask runs after ProcessEventQueue: read back the previous native
    // show/hide before issuing another. This pump never applies editor actions,
    // moves actors, starts quests, or executes deferred console/native commands.
    nativePauseRequest15_.reset();
    auto* player=RE::PlayerCharacter::GetSingleton();
    if (!ready_ || !player || !player->GetParentCell() || player->GetPlayerFlags().isLoading) {
        menuOpen_=false; closeRequested_=false;
    } else if (closeRequested_) {
        std::scoped_lock queueLock(queueMutex_);
        // Independent release path if the game task is temporarily delayed.
        // Pending edits still belong to the game task and must drain first.
        if (CanFinishClose(true,queue_.size(),batch_.size()+spawnJobs_.size())) {
            menuOpen_=false; closeRequested_=false; closeFinalization15_=true;
        }
    }
    SyncInput();
    SyncNativePause15();
}
void Engine::RequestTick() {
    auto* tasks=SKSE::GetTaskInterface();
    if (!tasks) return;
    // Main-task gameplay mutations preserve the original SKSE contract. The UI
    // pump has its own latch, so a delayed main task cannot starve pause release.
    // SKSE's legacy UIManager is CommonLib's UIMessageQueue. AddUITask
    // silently drops requests when that singleton is absent, so never acquire
    // a pending ticket before both native UI interfaces are available.
    if (RE::UI::GetSingleton() && RE::UIMessageQueue::GetSingleton()) {
        const auto ticket=nextPauseTick15_.fetch_add(1);
        std::uint64_t expected=0;
        if (pauseTickPending15_.compare_exchange_strong(expected,ticket)) {
            try { tasks->AddUITask([this,ticket] {
                struct Reset {
                    std::atomic<std::uint64_t>& pending; std::uint64_t ticket;
                    ~Reset() { auto expected=ticket; pending.compare_exchange_strong(expected,0); }
                } reset{pauseTickPending15_,ticket};
                std::scoped_lock stateLock(stateMutex_);
                // Load/revert may invalidate a queued callback. A stale callback
                // cannot mutate the new session or clear a newer task's ticket.
                if (pauseTickPending15_.load()!=ticket) return;
                try { PumpPause15(); }
                catch (const std::exception& e) {
                    menuOpen_=false; closeRequested_=true; SyncInput();
                    spdlog::error("Native pause lifecycle error; retrying release: {}",e.what());
                }
            }); } catch (...) {
                expected=ticket; pauseTickPending15_.compare_exchange_strong(expected,0); throw;
            }
        }
    } else {
        // If the native UI vanished, an admitted UI task may have been dropped.
        // Invalidate its ticket; a surviving stale callback cannot clear a retry.
        pauseTickPending15_=0;
    }
    if (tickPending_.exchange(true)) return;
    tasks->AddTask([this] {
        struct Reset { std::atomic<bool>& flag; ~Reset() { flag = false; } } reset{tickPending_};
        try { Tick(); }
        catch (const std::exception& e) {
            std::scoped_lock stateLock(stateMutex_);
            menuOpen_=false; closeRequested_=false;
            { std::scoped_lock lock(queueMutex_); queue_.clear(); }
            batch_.clear(); afterClose_.clear(); CancelActorSpawns(); SyncInput();
            Note(std::string("C++ 错误，已停止待处理操作并请求释放面板暂停：")+e.what());
        }
        // Access violations are deliberately NOT swallowed as a successful operation.
    });
}
void Engine::Tick() {
    std::scoped_lock stateLock(stateMutex_);
    auto now = Clock::now();
    float dt = lastTick_.time_since_epoch().count() ? std::chrono::duration<float>(now-lastTick_).count() : 0.016f;
    lastTick_ = now;
    auto* player = RE::PlayerCharacter::GetSingleton();
    auto* ui = RE::UI::GetSingleton();
    const bool available = ready_ && player && player->GetParentCell() && !player->GetPlayerFlags().isLoading;
    if (!available) {
        law11::SetPolicy(false,false);
        menuOpen_ = false; closeRequested_ = false;
        SyncInput(); RefreshView(); return;
    }
    SyncInput();
    TickKernel11(std::clamp(dt,0.0f,0.25f));
    // Our own native pause must not starve editor actions or the close drain.
    // Other native menus retain their pause and delay unsafe/deferred commands.
    if (OtherNativePause15()) {
        if (closeRequested_) { menuOpen_=false; closeRequested_=false; closeFinalization15_=true; SyncInput(); }
        RefreshView(); return;
    }
    // Native show/hide is asynchronous. Edits wait for the opening readback;
    // native-menu, teleport and rest actions wait for the closing readback.
    if ((menuOpen_ && !NativePauseOpen15()) ||
        (!menuOpen_ && (NativePauseOpen15() || nativePauseRequest15_.has_value()))) {
        if (closeRequested_) { menuOpen_=false; closeRequested_=false; closeFinalization15_=true; SyncInput(); }
        RefreshView(); return;
    }
    if (!menuOpen_ && (closeFinalization15_ || !afterClose_.empty())) {
        FinishClose();
        // A deferred native menu/teleport can change the current world. Do not
        // continue this frame using the earlier player/cell snapshot afterward.
        RefreshView(); return;
    }
    auto* main=RE::Main::GetSingleton();
    const bool simulationPaused = (ui && ui->GameIsPaused()) || (main && main->GetRuntimeData().freezeTime);
    std::deque<Action> work;
    {
        std::scoped_lock lock(queueMutex_);
        for (int n=0;n<32 && !queue_.empty();++n) { work.push_back(std::move(queue_.front())); queue_.pop_front(); }
    }
    for (auto& a : work) if (a.epoch == epoch_.load()) {
        if (a.afterClose || a.op==Op::Goto || a.op==Op::GoBookmark || a.op==Op::Return || a.op==Op::Flight || a.op==Op::SpawnActors || a.op==Op::SpawnRandom10 || a.op==Op::SpawnSoldiers10 || a.op==Op::DoorTransit10 || a.op==Op::RescuePlayer10 || a.op==Op::OwnUse11 || a.op==Op::Rest11) {
            afterClose_.push_back(std::move(a)); closeRequested_=true;
        } else Apply(a);
        dirty_=true;
    }
    // Bounded batches continue while our world pause is active; no waiting for an unpause to apply edits.
    auto batchStart=Clock::now();
    for (int n=0;n<batchPerTick_ && !batch_.empty();++n) {
        auto a=std::move(batch_.front()); batch_.pop_front();
        if (a.epoch == epoch_.load()) {
            auto* receiver=Ref(a.target); auto* actor=receiver ? receiver->As<RE::Actor>() : nullptr;
            auto* f=RE::TESForm::LookupByID(a.form);
            const int result=AddForm(f,a.count,false,actor);
            ++batchDone_;
            if (result>0) ++batchSent_; else if (result==0) ++batchSkipped_; else ++batchFailed_;
            if (batch_.empty()) Note(fmt::format("批量完成：处理 {}；提交 {}；已有/跳过 {}；不支持/失败 {}。",batchDone_,batchSent_,batchSkipped_,batchFailed_));
            dirty_=true; hudDirty_=true;
        }
        if (Clock::now()-batchStart > std::chrono::milliseconds(4)) break;
    }
    ProcessActorSpawns();
    ProcessBirths10(simulationPaused ? 0.0f : std::clamp(dt,0.0f,0.25f));
    ProcessClear10();
    MaintainRemovals(simulationPaused ? 0.0f : std::clamp(dt,0.0f,0.1f));
    RefillResources();
    MaintainQuests11(std::clamp(dt,0.0f,0.25f));
    if (!simulationPaused) {
        MaintainFlight(std::clamp(dt,0.0f,0.05f));
        if (now-lastMaintain_ > std::chrono::milliseconds(250)) {
            const float elapsed=std::min(0.5f,std::chrono::duration<float>(now-lastMaintain_).count());
            Maintain(elapsed); lastMaintain_=now;
        }
    } else lastMaintain_=now; // menus must not accumulate follower "stuck" time
    if (hudDirty_ || closeRefreshFrames_>0) { RefreshHUD(); hudDirty_=false; if (closeRefreshFrames_>0) --closeRefreshFrames_; }
    std::size_t queued=0;
    { std::scoped_lock lock(queueMutex_); queued=queue_.size(); }
    if (CanFinishClose(closeRequested_.load(),queued,batch_.size()+spawnJobs_.size())) FinishClose();
    if (dirty_ || now-lastView_>std::chrono::milliseconds(100)) {
        RefreshInspection(); RefreshView(); lastView_=now; dirty_=false;
    }
}
void Engine::FinishClose() {
    // Commit edits first, restore time/controls second, run native-menu/teleport commands last.
    RefreshHUD(); closeRefreshFrames_=3; closeFinalization15_=true;
    menuOpen_=false; closeRequested_=false; SyncInput();
    // Hiding an IMenu is queued, not synchronous. Preserve deferred operations
    // until the next UI-queue readback confirms removal, and any other pause ends.
    if (NativePauseOpen15() || nativePauseRequest15_.has_value() || OtherNativePause15()) return;
    closeFinalization15_=false;
    auto work=std::move(afterClose_); afterClose_.clear();
    for (const auto& a:work) if (a.epoch == epoch_.load()) Apply(a);
    dirty_=true;
    spdlog::info("Realtime7: edits drained; menu closed; world pause lease released.");
}
bool Engine::NativePauseOpen15() const {
    auto* ui=RE::UI::GetSingleton();
    return ui && ui->IsMenuOpen(PauseMenu15::MENU_NAME);
}
bool Engine::NativePauseConfirmed15() const {
    auto* ui=RE::UI::GetSingleton();
    if (!ui) return false;
    const auto own=ui->GetMenu(PauseMenu15::MENU_NAME);
    return own && own->OnStack() && own->PausesGame() && ui->GameIsPaused();
}
bool Engine::OtherNativePause15() const {
    auto* ui=RE::UI::GetSingleton();
    if (!ui) return false;
    const auto own=ui->GetMenu(PauseMenu15::MENU_NAME);
    const std::uint32_t ours=own && own->OnStack() && own->PausesGame() ? 1u : 0u;
    return ui->numPausesGame > ours;
}
void Engine::SyncNativePause15() {
    const bool capture=menuOpen_ && ready_;
    if (auto* ui=RE::UI::GetSingleton()) {
        if (pauseUI15_!=ui) {
            ui->Register(PauseMenu15::MENU_NAME,PauseMenu15::Create);
            pauseUI15_=ui; nativePauseRequest15_.reset(); nativePauseReadback15_.reset();
        }
        const bool onStack=ui->IsMenuOpen(PauseMenu15::MENU_NAME);
        const auto own=ui->GetMenu(PauseMenu15::MENU_NAME);
        const bool pauses=own && own->PausesGame();
        const std::array<std::uint32_t,4> readback{capture?1u:0u,onStack?1u:0u,pauses?1u:0u,ui->numPausesGame};
        if (!nativePauseReadback15_ || *nativePauseReadback15_!=readback) {
            spdlog::info("NativePause15 readback: wanted={}, ownOnStack={}, ownPausesGame={}, totalNativePauses={}, fullPauseConfirmed={}",
                capture,onStack,pauses,ui->numPausesGame,onStack && pauses && ui->GameIsPaused());
            nativePauseReadback15_=readback;
        }
        // An opposite pending request must be cancelled even if the current
        // stack already matches (open then close in the same UI queue cycle).
        if (NativePauseMessageNeeded15(capture,onStack,nativePauseRequest15_)) {
            if (auto* messages=RE::UIMessageQueue::GetSingleton()) {
                messages->AddMessage(RE::BSFixedString(PauseMenu15::MENU_NAME),
                    capture ? RE::UI_MESSAGE_TYPE::kShow : RE::UI_MESSAGE_TYPE::kHide,nullptr);
                nativePauseRequest15_=capture;
                const auto now=Clock::now();
                if (!nativePauseLoggedRequest15_ || *nativePauseLoggedRequest15_!=capture || now-nativePauseLogAt15_>std::chrono::seconds(2)) {
                    spdlog::info("NativePause15: queued {} (awaiting native stack readback)",capture?"show":"hide");
                    nativePauseLoggedRequest15_=capture; nativePauseLogAt15_=now;
                }
            }
        }
    }
}
void Engine::SyncAudioFreeze16() {
    const bool capture=menuOpen_ && ready_;
    RE::BGSSoundCategory* master{};
    if (auto* defaults=RE::BGSDefaultObjectManager::GetSingleton()) {
        if (auto** slot=defaults->GetObject<RE::BGSSoundCategory>(RE::DefaultObjectID::kMasterSoundCategory)) master=*slot;
    }
    audioAvailable16_=master!=nullptr;
    if (!master) {
        audioMuted16_=false;
        if (capture && !audioWarned16_) {
            spdlog::warn("Freeze16: master sound category unavailable; world pause remains active but audio mute could not be acquired.");
            audioWarned16_=true;
        }
        return;
    }
    audioWarned16_=false;
    const float observed=master->GetCategoryVolume();
    if (auto write=audioLease16_.Update(capture,observed)) {
        master->SetCategoryVolume(*write);
        const float readback=master->GetCategoryVolume();
        spdlog::info("Freeze16: master audio requested={:.3f}, readback={:.3f} (lease={}, previous={:.3f}).",*write,readback,audioLease16_.Held(),audioLease16_.Before());
        if (capture && std::abs(readback)>0.001f) spdlog::warn("Freeze16: master audio mute readback is non-zero; will reassert while F8 remains open.");
    }
    const float readback=master->GetCategoryVolume();
    audioMuted16_=capture && audioLease16_.Held() && std::isfinite(readback) && std::abs(readback)<=0.001f;
}
void Engine::SyncInput() {
    const bool capture=menuOpen_ && ready_;
    pauseMenu_=true; // Read-only legacy compatibility field, never a policy opt-out.
    // freezeTime only bridges the asynchronous native show. Release its bool
    // immediately after full-pause readback, so later external writers during a
    // long F8 session are not overwritten when the native menu finally closes.
    if (auto* main=RE::Main::GetSingleton()) {
        auto& paused=main->GetRuntimeData().freezeTime;
        if (auto write=pauseLease_.Update(capture && !NativePauseConfirmed15(),paused)) {
            paused=*write;
            spdlog::info("Realtime7: world freezeTime={}, ownsPause={}",paused,pauseLease_.Held());
        }
    }
    SyncAudioFreeze16();
    auto* controls=RE::ControlMap::GetSingleton();
    if (!controls) return;
    if (capture && !inputBlocked_) {
        controls->GetControlsState(controlsEnabled_,controlsStored_);
        controls->SetControlsState(0,controlsStored_); inputBlocked_=true;
    } else if (!capture && inputBlocked_) {
        controls->SetControlsState(controlsEnabled_,controlsStored_); inputBlocked_=false;
    }
}
void Engine::RefreshHUD() {
    auto* messages=RE::UIMessageQueue::GetSingleton();
    if (!messages) return;
    // Allocate through the game's message factory; the queue owns successful allocations.
    auto send=[&](RE::HUD_MESSAGE_TYPE type, RE::ObjectRefHandle target) {
        auto* raw=messages->CreateUIMessageData(RE::BSFixedString("HUDData"));
        if (!raw) return;
        auto* data=static_cast<RE::HUDData*>(raw);
        data->type=type; data->crosshairRef=target;
        messages->AddMessage(RE::BSFixedString("HUD Menu"),RE::UI_MESSAGE_TYPE::kUpdate,data);
    };
    send(RE::HUD_MESSAGE_TYPE::kRefreshAll,{});
    if (auto* pick=RE::CrosshairPickData::GetSingleton())
        send(RE::HUD_MESSAGE_TYPE::kSetCrosshairTarget,pick->GetActiveTarget());
}

RE::TESObjectREFR* Engine::Ref(ID id) const {
    if (!id) return nullptr;
    auto* f = RE::TESForm::LookupByID(id);
    return f ? f->AsReference() : nullptr;
}
void Engine::Run(std::string_view text, RE::TESObjectREFR* target) {
    if (!SafeCommand(text)) { Note("命令被拒绝：为空、多行或长度过长。"); return; }
    auto* factory = RE::IFormFactory::GetFormFactoryByType(RE::FormType::Script);
    auto* form = factory ? factory->Create() : nullptr;
    auto* script = form ? form->As<RE::Script>() : nullptr;
    if (!script) { if (form) delete form; Note("脚本工厂创建失败。"); return; }
    std::unique_ptr<RE::Script> owner(script);
    script->SetCommand(text);
    script->CompileAndRun(target);
    spdlog::debug("Command submitted [{}]: {}", target ? Hex(target->GetFormID()) : "GLOBAL", text);
    // CompileAndRun returns void. Submission is not proof that the command succeeded.
}
void Engine::Note(std::string text) {
    spdlog::info("{}", text);
    notes_.push_back(std::move(text));
    while (notes_.size() > 18) notes_.pop_front();
}
void Engine::BuildCatalog() {
    auto* data = RE::TESDataHandler::GetSingleton();
    if (!data) return;
    auto result = std::make_shared<std::vector<Entry>>();
    auto scan = [&]<class T>(Kind kind) {
        for (auto* form : data->GetFormArray<T>()) {
            if (!form || form->IsDeleted()) continue;
            Entry e;
            e.id = form->GetFormID(); e.kind = kind;
            const auto* name = form->GetName(); e.name = name ? name : "";
            const auto* editor = form->GetFormEditorID(); e.editor = editor ? editor : "";
            auto* file = form->GetFile(0); e.source = file ? file->fileName : "<runtime>";
            if (auto* npc=form->template As<RE::TESNPC>()) {
                auto* race=npc->race;
                if(race) { e.race=race->GetFormID(); e.raceName=Name(race); }
                else e.raceName="模板继承 / 未解析";
                e.unique=npc->IsUnique(); e.preset=npc->IsPreset();
            }
            e.leveled=kind==Kind::LeveledActor;
            e.search = e.name + " " + e.editor + " " + e.source + " " + Hex(e.id)+" "+e.raceName+" "+Hex(e.race);
            e.addable = form->IsInventoryObject() || kind==Kind::Spell || kind==Kind::Shout || kind==Kind::Word || kind==Kind::Perk;
            // NonPlayable is meaningful for equipment; do not hide quest/weather/magic records
            // through an unrelated base-class virtual default.
            if (kind==Kind::Armor || kind==Kind::Weapon)
                e.playable=(form->GetFormFlags() & RE::TESForm::RecordFlags::kNonPlayable)==0;
            result->push_back(std::move(e));
        }
    };
    scan.template operator()<RE::TESObjectARMO>(Kind::Armor);
    scan.template operator()<RE::TESObjectWEAP>(Kind::Weapon);
    scan.template operator()<RE::SpellItem>(Kind::Spell);
    scan.template operator()<RE::TESShout>(Kind::Shout);
    scan.template operator()<RE::TESWordOfPower>(Kind::Word);
    scan.template operator()<RE::BGSPerk>(Kind::Perk);
    scan.template operator()<RE::TESNPC>(Kind::NPC);
    scan.template operator()<RE::TESLevCharacter>(Kind::LeveledActor);
    scan.template operator()<RE::TESAmmo>(Kind::Ammo);
    scan.template operator()<RE::TESObjectBOOK>(Kind::Book);
    scan.template operator()<RE::AlchemyItem>(Kind::Potion);
    scan.template operator()<RE::ScrollItem>(Kind::Scroll);
    scan.template operator()<RE::TESObjectMISC>(Kind::Misc);
    scan.template operator()<RE::TESKey>(Kind::Key);
    scan.template operator()<RE::IngredientItem>(Kind::Ingredient);
    scan.template operator()<RE::TESObjectCONT>(Kind::Container);
    scan.template operator()<RE::TESFurniture>(Kind::Furniture);
    scan.template operator()<RE::TESWeather>(Kind::Weather);
    scan.template operator()<RE::TESQuest>(Kind::Quest);
    scan.template operator()<RE::TESObjectCELL>(Kind::Cell);
    scan.template operator()<RE::TESFaction>(Kind::Faction);
    scan.template operator()<RE::TESRace>(Kind::Race);
    scan.template operator()<RE::BGSOutfit>(Kind::Outfit);
    scan.template operator()<RE::TESPackage>(Kind::Package);
    scan.template operator()<RE::TESSoulGem>(Kind::SoulGem);
    scan.template operator()<RE::TESObjectLIGH>(Kind::Light);
    AnnotateCatalog10(*result);
    std::sort(result->begin(), result->end(), [](const Entry& a, const Entry& b) {
        if (a.kind != b.kind) return a.kind < b.kind;
        if (a.name != b.name) return a.name < b.name;
        return a.id < b.id;
    });
    { std::scoped_lock lock(catalogMutex_); catalog_ = result; }
    Note(fmt::format("目录已索引 {} 条已加载记录。", result->size()));
}
bool Engine::SetValue(RE::Actor* actor, const std::string& name, float value, bool effective) {
    if (!actor || !Identifier(name) || !std::isfinite(value)) { Note("无效的角色、属性名或数值。"); return false; }
    const auto av=RE::ActorValueList::LookupActorValueByName(name.c_str());
    if (av==RE::ActorValue::kNone) { Note("引擎不认识该角色属性："+name); return false; }
    if (av==RE::ActorValue::kSpeedMult) value=Finite(value,100,1,10000);
    if (actor->GetFormID()==kPlayer) {
        if (av==RE::ActorValue::kHealth && healthBoost_.enabled) SetBoost(healthBoost_,av,false,1000000);
        if (av==RE::ActorValue::kMagicka && magickaBoost_.enabled) SetBoost(magickaBoost_,av,false,1000000);
    }
    const float before=actor->GetActorValue(av), baseBefore=actor->GetBaseActorValue(av);
    const float runBefore=av==RE::ActorValue::kSpeedMult?actor->GetRunSpeed():0;
    auto* process=actor->GetActorRuntimeData().currentProcess;
    auto* cache=process ? process->cachedValues : nullptr;
    actor->SetBaseActorValue(av,DesiredBaseValue(before,baseBefore,value,effective));
    process=actor->GetActorRuntimeData().currentProcess; cache=process?process->cachedValues:nullptr;
    if (cache) {
        for (auto& entry:cache->actorValueCache) entry.invalid=true;
        for (auto& entry:cache->maxActorValueCache) entry.invalid=true;
    }
    const float after=actor->GetActorValue(av);
    if (av==RE::ActorValue::kSpeedMult) {
        // Trigger the engine's encumbrance/movement notification without a net carry-weight change.
        // Do not disable/re-enable the player, reload 3D, or teleport to force a stat refresh.
        actor->ModActorValue(RE::ACTOR_VALUE_MODIFIER::kTemporary,RE::ActorValue::kCarryWeight,1.0f);
        actor->ModActorValue(RE::ACTOR_VALUE_MODIFIER::kTemporary,RE::ActorValue::kCarryWeight,-1.0f);
        // The gait cache's relationship to race/movement-controller multipliers is not
        // guaranteed. Do NOT multiply raw cached speed fields or patch unknown validity bits.
        // Force normal getter evaluation after the native value-change notifications instead.
        (void)actor->GetWalkSpeed(); (void)actor->GetJogSpeed(); (void)actor->GetFastWalkSpeed();
        Note(fmt::format("速度读回：{:.1f}% → {:.1f}% | 跑速缓存 {:.1f} → {:.1f}（非位移测速；关闭面板后移动验证）。",before,after,runBefore,actor->GetRunSpeed()));
    } else Note(fmt::format("{}：当前值 {:.3f} → {:.3f}；基础值 {:.3f}",name,before,after,actor->GetBaseActorValue(av)));
    hudDirty_=dirty_=true;
    return true;
}
void Engine::SetBoost(ResourceBoost& state, RE::ActorValue av, bool enable, float limit) {
    auto* player=RE::PlayerCharacter::GetSingleton();
    if (!player || state.enabled==enable) return;
    if (enable) {
        state.original=player->GetBaseActorValue(av);
        const float maximum=player->GetActorValueMax(av);
        if (!std::isfinite(state.original) || !std::isfinite(maximum)) {
            Note("资源属性异常，未执行增量修改。"); return;
        }
        state.applied=BoostedBase(state.original,maximum,limit);
        player->SetBaseActorValue(av,state.applied);
        state.enabled=true;
    } else {
        // Preserve unrelated changes since enabling, instead of overwriting the complete stat.
        const float current=player->GetBaseActorValue(av);
        player->SetBaseActorValue(av,RestoreBoostedBase(current,state.original,state.applied));
        state.enabled=false;
    }
    RefillResources(); hudDirty_=dirty_=true;
    Note(enable ? "已启用高上限 + 每帧补满；非永久改写第三方魔法成本。" : "已撤销本功能添加的上限增量。");
}
void Engine::RefillResources() {
    auto* player=RE::PlayerCharacter::GetSingleton();
    if (!player || player->IsDead(false)) return;
    auto refill=[&](RE::ActorValue av) {
        const float current=player->GetActorValue(av), maximum=player->GetActorValueMax(av);
        if (std::isfinite(current) && std::isfinite(maximum) && maximum>current)
            player->RestoreActorValue(av,maximum-current);
    };
    if (unlimited_ || healthBoost_.enabled) refill(RE::ActorValue::kHealth);
    if (unlimited_ || magickaBoost_.enabled) refill(RE::ActorValue::kMagicka);
    if (unlimited_) refill(RE::ActorValue::kStamina);
}

void Engine::Own(RE::TESObjectREFR* ref) {
    auto* player = RE::PlayerCharacter::GetSingleton();
    if (!ref || !player || ref->As<RE::Actor>()) return;
    ref->SetActivationBlocked(false);
    ProtectObject11(ref,true);
    ref->SetOwner(player->GetActorBase());
    ref->AddChange(RE::TESObjectREFR::ChangeFlags::kOwnershipExtra);
    if (ref->IsLocked()) Run("unlock", ref);
    hudDirty_=dirty_=true;
}
void Engine::Claim(RE::TESObjectCELL* cell, int eviction, ID exit) {
    if (!cell || !cell->IsInteriorCell()) {
        Note("请选择带室内传送链接的门，或当前室内 Cell；室外世界 Cell 不能作为房屋接管。"); return;
    }
    ClaimState claim{cell->GetFormID(), std::clamp(eviction, 0, 3), exit};
    claims_[claim.cell] = claim;
    claimedThisVisit_.erase(claim.cell);
    ConfigureClaim(claim);
    if (cell->IsAttached()) ApplyClaim(claim);
    Note("房屋已归你，名称 keqing；区域和入口立即更新。尚未实例化的室内对象在加载时接管。");
}
void Engine::ConfigureClaim(const ClaimState& claim) {
    auto* cell=RE::TESForm::LookupByID<RE::TESObjectCELL>(claim.cell);
    auto* player=RE::PlayerCharacter::GetSingleton();
    if (!cell || !player || !cell->IsInteriorCell()) return;
    // Change the actual CELL full name, not the shared base form of every door.
    renamedCells_.try_emplace(claim.cell,cell->fullName.c_str());
    cell->SetFullName(claim.name.c_str());
    {
        std::scoped_lock lock(catalogMutex_);
        if (catalog_) {
            const auto it=std::find_if(catalog_->begin(),catalog_->end(),[&](const Entry& e){return e.id==claim.cell;});
            if (it!=catalog_->end() && it->name!=claim.name) {
                auto changed=std::make_shared<std::vector<Entry>>(*catalog_);
                auto& entry=(*changed)[static_cast<std::size_t>(std::distance(catalog_->begin(),it))];
                entry.name=claim.name; entry.search=entry.name+" "+entry.editor+" "+entry.source+" "+Hex(entry.id);
                catalog_=std::move(changed);
            }
        }
    }
    cell->AddChange(RE::TESObjectCELL::ChangeFlags::kFullName);
    cell->SetOwner(player->GetActorBase()); cell->SetPublic(true);
    if (auto* entrance=Ref(claim.exit)) {
        Own(entrance); entrance->SetDisplayName(RE::BSFixedString(claim.name.c_str()),true);
    }
    // Every accessible exterior entrance into this CELL receives the same per-reference name.
    for (ID id:CellRefs(cell)) if (auto* door=Ref(id)) {
        auto* extra=door->extraList.GetByType<RE::ExtraTeleport>();
        auto linked=extra && extra->teleportData ? extra->teleportData->linkedDoor.get() : RE::NiPointer<RE::TESObjectREFR>{};
        if (linked && linked->GetParentCell() && linked->GetParentCell()->IsExteriorCell()) {
            Own(linked.get()); linked->SetDisplayName(RE::BSFixedString(claim.name.c_str()),true);
        }
    }
    hudDirty_=dirty_=true;
}
void Engine::ApplyClaim(const ClaimState& claim) {
    auto* cell = RE::TESForm::LookupByID<RE::TESObjectCELL>(claim.cell);
    auto* player = RE::PlayerCharacter::GetSingleton();
    if (!cell || !cell->IsAttached() || !player) return;
    ConfigureClaim(claim);
    auto refs = CellRefs(cell);
    for (ID id : refs) {
        if (id == kPlayer) continue;
        auto* ref = Ref(id);
        if (!ref || ref->IsDeleted()) continue;
        if (auto* actor = ref->As<RE::Actor>()) {
            if (!followers_.contains(id)) Evict(actor, claim.eviction, Ref(claim.exit));
        } else Own(ref);
    }
    claimedThisVisit_.insert(claim.cell);
}
void Engine::Evict(RE::Actor* actor, int mode, RE::TESObjectREFR* exit) {
    if (!actor || actor->GetFormID() == kPlayer || mode == 0) return;
    if (mode == 1) {
        if (exit && exit->GetParentCell() != actor->GetParentCell()) {
            actor->StopCombat(); actor->MoveTo(exit); actor->MoveToNearestNavmesh();
        } // No fabricated destination: relocation needs an outside reference.
    } else if (mode == 2) actor->Disable();
    else RemoveReference(actor);
}
// Returns 1=submitted / accepted, 0=already present, -1=unsupported / rejected.
int Engine::AddForm(RE::TESForm* form, int count, bool remove, RE::Actor* receiver) {
    if (!form || !receiver || form->IsDeleted()) return -1;
    count=std::clamp(count,1,1000000);
    const auto id=Hex(form->GetFormID());
    if (auto* perk=form->As<RE::BGSPerk>()) {
        if (remove) receiver->RemovePerk(perk);
        else if (receiver->HasPerk(perk)) return 0;
        else receiver->AddPerk(perk);
        return receiver->HasPerk(perk)!=remove ? 1 : -1;
    }
    if (auto* spell=form->As<RE::SpellItem>()) {
        if (remove) return receiver->RemoveSpell(spell) ? 1 : 0;
        if (receiver->HasSpell(spell)) return 0;
        return receiver->AddSpell(spell) ? 1 : -1;
    }
    if (auto* shout=form->As<RE::TESShout>()) {
        if (remove) { Run("removeshout "+id,receiver); return 1; }
        const bool existed=receiver->HasShout(shout);
        if (!existed && !receiver->AddShout(shout)) return -1;
        if (receiver->GetFormID()==kPlayer) for (const auto& variation:shout->variations) if (variation.word) {
            const auto wordID=Hex(variation.word->GetFormID());
            Run("teachword "+wordID); Run("unlockword "+wordID);
        }
        return 1; // existing shouts also need their linked words unlocked

    }
    if (form->As<RE::TESWordOfPower>()) {
        if (remove || receiver->GetFormID()!=kPlayer) return -1;
        Run("teachword "+id); Run("unlockword "+id); return 1;
    }
    if (form->IsInventoryObject()) if (auto* object=form->As<RE::TESBoundObject>()) {
        if (remove) Run(fmt::format("removeitem {} {}",id,count),receiver);
        else receiver->AddObjectToContainer(object,nullptr,count,nullptr);
        return 1;
    }
    return -1;
}
void Engine::PlanBatch(const Action& a) {
    if (!batch_.empty()) { Note("上一批尚未完成；请等待或取消后再提交，避免重复添加。"); return; }
    auto catalog=Catalog();
    auto* ref=Ref(a.target ? a.target : kPlayer); auto* actor=ref ? ref->As<RE::Actor>() : nullptr;
    if (!catalog || !actor) { Note("批量接收者不是有效角色。"); return; }
    const auto inventory=actor->GetInventoryCounts(); // one snapshot, not O(N squared) inventory scans
    batchTotal_=batchDone_=batchSent_=batchSkipped_=batchFailed_=0;
    std::unordered_set<ID> unique;
    const int quantity=static_cast<int>(Finite(a.value[1],1,1,1000000));
    for (const auto& e:*catalog) {
        // -1 means ALL inventory categories, not weather/quests or placeable containers.
        bool kindMatch=a.count==-1 ? (e.kind==Kind::Armor || e.kind==Kind::Weapon || e.kind==Kind::Ammo ||
            e.kind==Kind::Book || e.kind==Kind::Potion || e.kind==Kind::Scroll || e.kind==Kind::Misc ||
            e.kind==Kind::Key || e.kind==Kind::Ingredient || e.kind==Kind::SoulGem || e.kind==Kind::Light) : static_cast<int>(e.kind)==a.count;
        if (!kindMatch || !e.addable || !Contains(e.search,a.text) || (!a.source.empty() && e.source!=a.source) ||
            (a.value[0]==0 && e.name.empty()) || (a.playableOnly && !e.playable) || !unique.insert(e.id).second) continue;
        auto* form=RE::TESForm::LookupByID(e.id); if (!form) { ++batchFailed_; continue; }
        int n=quantity;
        if (auto* bound=form->As<RE::TESBoundObject>(); bound && form->IsInventoryObject()) {
            auto it=inventory.find(bound); n=MissingQuantity(quantity,it==inventory.end()?0:it->second,a.onlyMissing);
        } else if (a.onlyMissing) {
            if (auto* spell=form->As<RE::SpellItem>(); spell && actor->HasSpell(spell)) n=0;
            if (auto* perk=form->As<RE::BGSPerk>(); perk && actor->HasPerk(perk)) n=0;
            // Do not skip existing shouts: linked words may still be locked.
        }
        ++batchTotal_;
        if (n<=0) { ++batchSkipped_; ++batchDone_; continue; }
        batch_.push_back(Action{.op=Op::Add,.target=actor->GetFormID(),.form=e.id,.count=n,.epoch=a.epoch});
    }
    Note(fmt::format("已筛选 {} 条；跳过已有 {}；排队 {}。菜单关闭会等待本批应用完成，也可取消余项。",batchTotal_,batchSkipped_,batch_.size()));
}
void Engine::RefreshInspection() {
    inspectedCount_=0; inspectedKnown_=false; inspectedAddable_=false;
    auto* player=RE::PlayerCharacter::GetSingleton(); auto* f=RE::TESForm::LookupByID(inspected_);
    if (!player || !f || !ready_) return;
    if (auto* bound=f->As<RE::TESBoundObject>(); bound && f->IsInventoryObject()) {
        const auto counts=player->GetInventoryCounts([&](RE::TESBoundObject& object){ return &object==bound; });
        if (auto it=counts.find(bound);it!=counts.end()) inspectedCount_=std::max(0,it->second);
        inspectedKnown_=inspectedCount_>0; inspectedAddable_=true;
    } else if (auto* spell=f->As<RE::SpellItem>()) { inspectedKnown_=player->HasSpell(spell); inspectedAddable_=true; }
    else if (auto* perk=f->As<RE::BGSPerk>()) { inspectedKnown_=player->HasPerk(perk); inspectedAddable_=true; }
    else if (auto* shout=f->As<RE::TESShout>()) { inspectedKnown_=player->HasShout(shout); inspectedAddable_=true; }
    else if (auto* word=f->As<RE::TESWordOfPower>()) { inspectedAddable_=true; inspectedKnown_=word->GetKnown(); }
}

ID Engine::MarkerAtPlayer(ID reuse) {
    auto* player = RE::PlayerCharacter::GetSingleton();
    if (!player) return 0;
    if (auto* marker = Ref(reuse)) { marker->MoveTo(player); return reuse; }
    auto* base = RE::TESForm::LookupByID<RE::TESBoundObject>(0x0000003B); // Skyrim.esm XMarker.
    if (!base) return 0;
    auto marker = player->PlaceObjectAtMe(base, true);
    return marker ? marker->GetFormID() : 0;
}
void Engine::TeleportPlayer(RE::TESObjectREFR* target) {
    auto* player = RE::PlayerCharacter::GetSingleton();
    if (!target || !player || target == player) return;
    menuOpen_ = false; SyncInput();
    lastMarker_ = MarkerAtPlayer(lastMarker_);
    player->MoveTo(target);
}
void Engine::MaintainFlight(float dt) {
    if (!flight_ || menuOpen_) return;
    auto* player = RE::PlayerCharacter::GetSingleton();
    if (!player) return;
    // Horizontal/pitch movement is provided by vanilla TCL. Page Up/Down adds vertical motion.
    // Require foreground game window to avoid motion while Alt-Tabbed.
    DWORD pid{}; GetWindowThreadProcessId(GetForegroundWindow(), &pid);
    if (pid != GetCurrentProcessId()) return;
    float vertical = 0;
    if (GetAsyncKeyState(VK_PRIOR) & 0x8000) vertical += 1;
    if (GetAsyncKeyState(VK_NEXT) & 0x8000) vertical -= 1;
    if (vertical != 0) {
        auto p = player->GetPosition();
        p.z += vertical * flightSpeed_ * dt * ((GetAsyncKeyState(VK_SHIFT) & 0x8000) ? 3.0f : 1.0f);
        player->SetPosition(p, true);
    }
}
void Engine::Maintain(float dt) {
    auto* player = RE::PlayerCharacter::GetSingleton();
    if (!player) return;
    auto* processes = RE::ProcessLists::GetSingleton();
    if (processes) {
        if ((peace_ || ignore_) && !detectionCaptured_) {
            detectionBefore_ = processes->runDetection; detectionCaptured_ = true;
        }
        if (peace_ || ignore_) processes->runDetection = false;
        else if (detectionCaptured_) { processes->runDetection = detectionBefore_; detectionCaptured_ = false; }
    }
    if (peace_ && processes) processes->StopCombatAndAlarmOnActor(player, true);

    auto actors = Actors();
    for (ID id : actors) {
        auto* ref = Ref(id); auto* actor = ref ? ref->As<RE::Actor>() : nullptr;
        if (!actor) continue;
        if (peace_ && processes && actor->IsInCombat()) processes->StopCombatAndAlarmOnActor(actor, true);
        if (freeze_) {
            frozen_.try_emplace(id, actor->IsAIEnabled());
            actor->EnableAI(false);
        }
    }
    if (!freeze_ && !frozen_.empty()) {
        for (auto it = frozen_.begin(); it != frozen_.end();) {
            auto* ref = Ref(it->first); auto* actor = ref ? ref->As<RE::Actor>() : nullptr;
            if (actor) { actor->EnableAI(it->second); it = frozen_.erase(it); }
            else ++it;
        }
    }
    MaintainFreedom10(dt,actors);
    MaintainKernel11(dt,actors);
    MaintainLegion(dt,actors);
    for (const auto& [id,oldAggression]:pacified_) {
        (void)oldAggression;
        auto* ref=Ref(id); auto* actor=ref?ref->As<RE::Actor>():nullptr;
        if (!actor || actor->IsDead(false)) continue;
        if (actor->GetBaseActorValue(RE::ActorValue::kAggression)!=0) actor->SetBaseActorValue(RE::ActorValue::kAggression,0);
        if (actor->IsInCombat()) {
            actor->StopCombat();
            if (processes) processes->StopCombatAndAlarmOnActor(actor,true);
        }
    }

    auto* cell = player->GetParentCell();
    if (cell) {
        auto id = cell->GetFormID();
        if (auto it = claims_.find(id); it != claims_.end()) {
            if (!claimedThisVisit_.contains(id)) ApplyClaim(it->second);
            // Keep actual intruders out while this claimed cell is active.
            else if (it->second.eviction) {
                for (ID actorID : CellRefs(cell)) {
                    auto* ref = Ref(actorID); auto* actor = ref ? ref->As<RE::Actor>() : nullptr;
                    if (actor && !followers_.contains(actorID)) Evict(actor, it->second.eviction, Ref(it->second.exit));
                }
            }
        }
        std::erase_if(claimedThisVisit_, [id](ID previous) { return previous != id; });
    }
}
void Engine::Apply(const Action& a) {
    if(a.op==Op::SexLabAdd16 || a.op==Op::SexLabRemove16 || a.op==Op::SexLabMove16 || a.op==Op::SexLabClear16 || a.op==Op::SexLabStart16 || a.op==Op::SexLabStopAll16) { ApplySexLab16(a); return; }
    if(a.op==Op::ResourceMax12 || a.op==Op::ResourcePercent12 || a.op==Op::PlayerLevel12) { ApplyPlayer12(a); return; }
    if(a.op>=Op::KernelRules11) {
        if(a.op>=Op::QuestSelect11 && a.op<=Op::PersonalGoal11)ApplyQuest11(a);
        else ApplyKernel11(a);
        return;
    }
    if(a.op==Op::QuestStage || a.op==Op::QuestStart || a.op==Op::QuestStop || a.op==Op::QuestReset || a.op==Op::QuestComplete) {ApplyQuest11(a);return;}
    auto* player = RE::PlayerCharacter::GetSingleton();
    if (!player) return;
    auto* ref = Ref(a.target);
    auto* actor = ref ? ref->As<RE::Actor>() : nullptr;
    auto* form = a.form ? RE::TESForm::LookupByID(a.form) : nullptr;
    const auto value = [&](std::size_t i, float def, float lo, float hi) { return Finite(a.value[i], def, lo, hi); };
    switch (a.op) {
    case Op::Select: selected_ = a.target; break;
    case Op::SelectCrosshair: {
        auto* cross = RE::CrosshairPickData::GetSingleton();
        if (cross) { auto handle = cross->GetActiveTarget(); auto target = handle.get(); if (target) selected_ = target->GetFormID(); }
        break;
    }
    case Op::SelectPlayer: selected_ = kPlayer; break;
    case Op::Command:
        if (!a.target || ref) { Run(a.text, ref); Note("命令已提交（不代表一定成功）：" + a.text); }
        else Note("目标引用当前不可用。");
        break;
    case Op::Unlock: if (ref) Run("unlock", ref); break;
    case Op::Lock:
        if(ref)ProtectObject11(ref,false); if (ref) Run(fmt::format("lock {}", std::clamp(a.count, 0, 255)), ref); break;
    case Op::Own: Own(ref); break;
    case Op::Enable: if (ref && a.target != kPlayer) ref->Enable(false); break;
    case Op::Disable: if (ref && a.target != kPlayer) ref->Disable(); break;
    case Op::Delete: case Op::Banish: RemoveReference(ref); break;
    case Op::Move:
        if (ref) { auto p = ref->GetPosition(); p.x += value(0,0,-100000,100000); p.y += value(1,0,-100000,100000); p.z += value(2,0,-100000,100000); if (actor) actor->SetPosition(p,true); else ref->SetPosition(p); }
        break;
    case Op::Rotate:
        if (ref) { auto p = ref->GetAngle(); constexpr float radians = 0.017453292519943295f;
            p.x += value(0,0,-36000,36000)*radians; p.y += value(1,0,-36000,36000)*radians; p.z += value(2,0,-36000,36000)*radians; ref->SetAngle(p); }
        break;
    case Op::Scale: if (ref) ref->SetScale(value(0,1,0.01f,100)); break;
    case Op::Collision: if (ref) ref->SetCollision(a.count != 0); break;
    case Op::Clone:
        if (ref && ref->GetBaseObject()) {
            auto clone = ref->PlaceObjectAtMe(ref->GetBaseObject(), true);
            if (clone) { auto p = ref->GetPosition(); p.x += 100; clone->SetPosition(p); selected_ = clone->GetFormID(); }
        } break;
    case Op::ClaimDoor:
        if (ref) {
            auto* teleport = ref->extraList.GetByType<RE::ExtraTeleport>();
            auto linked = teleport && teleport->teleportData ? teleport->teleportData->linkedDoor.get() : RE::NiPointer<RE::TESObjectREFR>{};
            if (linked && linked->GetParentCell() && linked->GetParentCell()->IsInteriorCell()) {
                Own(ref); Own(linked.get()); Claim(linked->GetParentCell(), a.count, ref->GetFormID());
            } else Note("该目标没有连接室内区域的门；建筑外观本身不是室内 Cell 链接。");
        } break;
    case Op::ClaimCell: {
        auto* cell = player->GetParentCell(); ID exit = 0;
        if (cell) for (ID id : CellRefs(cell)) {
            auto* door = Ref(id); auto* extra = door ? door->extraList.GetByType<RE::ExtraTeleport>() : nullptr;
            auto linked = extra && extra->teleportData ? extra->teleportData->linkedDoor.get() : RE::NiPointer<RE::TESObjectREFR>{};
            if (linked && linked->GetParentCell() && linked->GetParentCell()->IsExteriorCell()) { exit = linked->GetFormID(); break; }
        }
        if (a.count == 1 && !exit) Note("未找到室外出口：所有权会生效，但无法执行驱逐搬移。");
        Claim(cell, a.count, exit); break;
    }
    case Op::Evict: Evict(actor, a.count, Ref(a.form)); break;
    case Op::Follow: Follow(actor); break;
    case Op::Wait:
        WaitFollower(a.target,a.count!=0); break;
    case Op::Dismiss: Dismiss(a.target); break;
    case Op::Summon: if (ref && ref != player) ref->MoveTo(player); break;
    case Op::Goto: TeleportPlayer(ref); break;
    case Op::StopCombat: if (actor) { actor->StopCombat(); if (auto* p = RE::ProcessLists::GetSingleton()) p->StopCombatAndAlarmOnActor(actor, true); } break;
    case Op::SetAI: if (actor && actor != player) actor->EnableAI(a.count != 0); break;
    case Op::Peace: peace_ = a.count != 0; Maintain(); break;
    case Op::Ignore: ignore_ = a.count != 0; Maintain(); break;
    case Op::FreezeAI: freeze_ = a.count != 0; Maintain(); break;
    case Op::Add: case Op::Remove: {
        auto* receiver=a.target?actor:player;
        const int result=AddForm(form,a.count,a.op==Op::Remove,receiver);
        if (result<0) Note("该目标/记录不支持此操作，或引擎拒绝了修改。");
        else Note(result==0 ? "已经拥有 / 学会，无需重复操作。" : "添加/移除已执行；所选目录记录会自动读回库存/学习状态。");
        inspected_=a.form; hudDirty_=true; break;
    }
    case Op::Spawn:
        if (form && (form->As<RE::TESNPC>() || form->As<RE::TESLevCharacter>())) { QueueActorSpawn(a); break; }
        if (form) if (auto* base = form->As<RE::TESBoundObject>()) {
            for (int i = 0; i < std::clamp(a.count, 1, 64); ++i) {
                auto spawned = player->PlaceObjectAtMe(base, true);
                if (spawned) {
                    auto p = player->GetPosition(); auto offset = LocalOffset(player->GetAngle().z, 250.0f+120*(i%8), 120.0f*(i/8), 0);
                    p.x += offset[0]; p.y += offset[1]; spawned->SetPosition(p); selected_ = spawned->GetFormID();
                }
            }
        } break;
    case Op::Equip: if (form && (!a.target || actor)) Run("equipitem " + Hex(a.form) + " 1", a.target ? actor : player); break;
    case Op::AddAll: PlanBatch(a); break;

    case Op::Skills: {
        constexpr const char* skills[] = {"onehanded","twohanded","marksman","block","smithing","heavyarmor","lightarmor","pickpocket","lockpicking","sneak","alchemy","speechcraft","alteration","conjuration","destruction","illusion","restoration","enchanting"};
        for (auto* skill : skills) SetValue(player,skill,static_cast<float>(std::clamp(a.count,0,1000)),false);
        Note("全部 18 项技能基础值已设置；Perk 需要单独添加。"); break;
    }
    case Op::CancelBatch: batch_.clear(); Note("已取消剩余批量任务；已经完成的修改会保留。"); break;
    case Op::SaveBookmark:
        if (bookmarks_.size() < 256) { auto id = MarkerAtPlayer(); if (id) bookmarks_.push_back({id, a.text.empty() ? Name(player->GetParentCell()) : a.text.substr(0,128)}); }
        break;
    case Op::GoBookmark: TeleportPlayer(Ref(a.form)); break;
    case Op::RemoveBookmark:
        std::erase_if(bookmarks_, [&](const Bookmark& b) { return b.marker == a.form; });
        if (auto* marker = Ref(a.form)) { marker->Disable(); Run("markfordelete", marker); }
        break;
    case Op::Return:
        if (auto* marker = Ref(lastMarker_)) {
            const auto destination = lastMarker_;
            lastMarker_ = MarkerAtPlayer();
            menuOpen_ = false; SyncInput(); player->MoveTo(marker);
            if (auto* old = Ref(destination)) { old->Disable(); Run("markfordelete", old); }
        } break;
    case Op::Weather: if (form) if (auto* weather = form->As<RE::TESWeather>()) if (auto* sky = RE::Sky::GetSingleton()) sky->ForceWeather(weather, true); break;
    case Op::ReleaseWeather: if (auto* sky = RE::Sky::GetSingleton()) sky->ReleaseWeatherOverride(); break;
    case Op::QuestStage:
        if (form && form->As<RE::TESQuest>()) Run(fmt::format("setstage {} {}", Hex(a.form), std::clamp(a.count,0,65535)));
        break;
    case Op::QuestStart: if (form && form->As<RE::TESQuest>()) Run("startquest " + Hex(a.form)); break;
    case Op::QuestStop: if (form && form->As<RE::TESQuest>()) Run("stopquest " + Hex(a.form)); break;
    case Op::QuestReset: if (form && form->As<RE::TESQuest>()) Run("resetquest " + Hex(a.form)); break;
    case Op::QuestComplete: if (form && form->As<RE::TESQuest>()) Run("completequest " + Hex(a.form)); break;
    case Op::AddFaction: if (actor && form) if (auto* faction = form->As<RE::TESFaction>()) actor->AddToFaction(faction, static_cast<std::int8_t>(std::clamp(a.count,-1,127))); break;
    case Op::RemoveFaction: if (actor && form && form->As<RE::TESFaction>()) Run("removefromfaction " + Hex(a.form), actor); break;
    case Op::FactionRelation: {
        auto* left = RE::TESForm::LookupByID<RE::TESFaction>(a.target);
        auto* right = form ? form->As<RE::TESFaction>() : nullptr;
        if (left && right) { if (a.count) left->SetEnemy(right,false,false); else left->SetAlly(right,false,false); }
        break;
    }
    case Op::ClearBounties: ClearCrime11(true); Note("已清除各阵营赏金；守卫豁免可在自由内核持续开启。"); break;
    case Op::SetRace: if (actor && form && form->As<RE::TESRace>()) Run("setrace " + Hex(a.form), actor); break;
    case Op::Outfit: if (actor && form) if (auto* outfit=form->As<RE::BGSOutfit>()) { actor->SetDefaultOutfit(outfit,true); Note("已设置角色套装并请求刷新 3D；未执行清空库存。"); } break;
    case Op::Rename: if (ref && !a.text.empty()) ref->SetDisplayName(RE::BSFixedString(a.text.c_str()), true); break;
    case Op::Settings:
        followDistance_ = FollowBands10[FollowBand10(value(0,240,64,10000))]; teleportDistance_ = std::max(value(1,4000,200,100000),followDistance_+100);
        flightSpeed_ = value(2,1000,10,20000); dragonsIndoors_ = a.count != 0;
        for(auto& [id,state]:followers_) {
            state.retrySeconds=0; state.package=nullptr; state.aliasRefreshPending15=true;
            if(!freeze_ && !state.waiting) if(auto* r=Ref(id)) if(auto* ac=r->As<RE::Actor>()) {
                BindFollower10(ac,state);
                if(ac->Get3D() && !ac->IsInCombat() && ac->GetActorRuntimeData().currentProcess) {
                    // One explicit settings change: retire only our fallback so
                    // the newly selected distance package can win naturally.
                    if(runtime10::IsOwnPackage(ac->GetCurrentPackage())) ac->EndInterruptPackage(false);
                    ac->EvaluatePackage(false,false);
                    state.aliasRefreshPending15=false;
                }
            }
        }
        Note(fmt::format("自然跟随基准距离已设为 {:.0f}；三组成员错开少量距离。",followDistance_));
        break;
    case Op::Flight:
        if (flight_ != (a.count != 0)) {
            menuOpen_ = false; SyncInput();
            Run("tcl"); flight_ = a.count != 0;
            Note("TCL 已切换。自由飞行启用期间不要在其他地方重复切换 TCL；Page Up/Down 控制垂直移动。");
        } break;
    case Op::Unlimited: unlimited_ = a.count != 0; break;
    case Op::CloseUI: closeRequested_=true; break;
    case Op::PauseMenu: pauseMenu_=true; SyncInput(); break; // Legacy action ID retained; F8 pause is mandatory.
    case Op::UISettings: fontScale_=value(0,1,0.8f,1.6f); break;
    case Op::SetValue: SetValue(a.target?actor:player,a.text,value(0,0,-1000000,1000000),a.count!=0); break;
    case Op::SuperHealth: SetBoost(healthBoost_,RE::ActorValue::kHealth,a.count!=0,1000000); break;
    case Op::InfiniteMagicka: SetBoost(magickaBoost_,RE::ActorValue::kMagicka,a.count!=0,1000000); break;
    case Op::Kill: ForceDeath(actor,a.count!=0); break;
    case Op::ForceControl:
        // Re-enter the same recruitment path even for an existing member: release
        // our wait/pacify state, but preserve the original state used on dismissal.
        if (actor && actor!=player) Follow(actor);
        break;
    case Op::LegionOrder: ApplyLegionOrder(a); break;
    case Op::LegionStats: {
        auto profile=a.profile; profile.Normalize();
        if (a.count) {
            for (auto& [id,state]:followers_) {
                state.profile=profile; state.profileEnabled=true; state.controlTimer=0;
                if (auto* r=Ref(id)) if(auto* ac=r->As<RE::Actor>()) SetMemberStats(ac,profile,true);
            }
            Note("已更新军团成员属性；不修改共享 NPC 模板。");
        } else if (actor && actor!=player) {
            if (auto it=followers_.find(a.target);it!=followers_.end()) { it->second.profile=profile; it->second.profileEnabled=true; }
            SetMemberStats(actor,profile,true); Note("目标属性已修改，面板显示读回值。");
        } else Note("请选择一个非玩家角色。");
        break;
    }
    case Op::LegionOptions:
        autoFight_=a.value[0]!=0; hardControl_=a.value[1]!=0;
        if (isolateFactions_!=(a.value[2]!=0)) {
            isolateFactions_=a.value[2]!=0;
            for(auto& [id,state]:followers_) if (auto* r=Ref(id)) if(auto* ac=r->As<RE::Actor>()) {
                if(isolateFactions_) IsolateActorFactions(ac,state); else RestoreActorFactions(ac,state);
            }
        }
        battleRadius_=Finite(a.value[3],5000,256,20000);
        for(auto& [id,state]:followers_) { state.controlTimer=0; state.retrySeconds=0; }
        break;
    case Op::SpawnActors: QueueActorSpawn(a); break;
    case Op::CancelSpawn: CancelActorSpawns(); Note("已取消尚未生成的部分；已经生成的角色保留。"); break;
    case Op::ClearSpawned: {
        const auto tracked=spawned_; // immutable snapshot; RemoveReference edits the live maps
        for(const auto& [id,entry]:tracked) {
            if(auto* r=Ref(id)) {
                auto* base=r->GetBaseObject();
                if(base && MatchesRemoval(id,entry.base,base->GetFormID())) RemoveReference(r);
            } else if(entry.base && id!=kPlayer) {
                removed_[id]=entry; followers_.erase(id); pacified_.erase(id); frozen_.erase(id); deathJobs_.erase(id);
            }
        }
        spawned_.clear(); Note("本工具生成实例已移除 / 登记后续加载抑制；未处理原地图同名角色。"); break;
    }
    case Op::RebuildCatalog: BuildCatalog(); break;
    case Op::Pacify:
        if (actor && actor!=player) {
            if (a.count) {
                pacified_.try_emplace(a.target,followers_.contains(a.target)?followers_.at(a.target).oldAggression:actor->GetBaseActorValue(RE::ActorValue::kAggression));
                actor->SetBaseActorValue(RE::ActorValue::kAggression,0);
                actor->StopCombat();
                if (auto* process=RE::ProcessLists::GetSingleton()) process->StopCombatAndAlarmOnActor(actor,true);
                Note("已持续安抚目标（不冻结行走/跟随）；脚本强制攻击仍可能覆盖。");
            } else if (auto it=pacified_.find(a.target);it!=pacified_.end()) {
                actor->SetBaseActorValue(RE::ActorValue::kAggression,followers_.contains(a.target)?0.0f:it->second); pacified_.erase(it);
            }
        } break;
    case Op::StopNearbyCombat:
        for (ID id:Actors()) if (auto* r=Ref(id)) if (auto* ac=r->As<RE::Actor>()) {
            ac->StopCombat(); if (auto* process=RE::ProcessLists::GetSingleton()) process->StopCombatAndAlarmOnActor(ac,true);
        }
        Note("已停止当前处理中的角色战斗。"); break;
    case Op::FollowMode: forcedFollow_=a.count!=0; for (auto& [id,state]:followers_) { state.progress.Reset(); state.goalProgress.Reset(); state.retrySeconds=0; } break;
    case Op::Inspect: inspected_=a.form; RefreshInspection(); break;
    case Op::SpawnRandom10:case Op::SpawnSoldiers10:case Op::DoorTransit10:case Op::ClearPulse10:
    case Op::ClearWorld10:case Op::ClearConfig10:case Op::ClearCancel10:case Op::LearnPower10:
    case Op::CastPower10:case Op::RescuePlayer10:case Op::FreedomSettings10:case Op::RefreshTravel10:case Op::LoadedActor10:
        ApplyFreedom10(a);break;
    }
    dirty_=true;
    if (a.op!=Op::Select && a.op!=Op::Inspect && a.op!=Op::SelectCrosshair) hudDirty_=true;
}
void Engine::RefreshView() {
    View next;
    next.ready = ready_.load(); next.epoch = epoch_.load();
    next.peace = peace_; next.ignore = ignore_; next.freeze = freeze_; next.flight = flight_; next.unlimited = unlimited_;
    next.dragonsIndoors = dragonsIndoors_;
    next.flightSpeed = flightSpeed_; next.followDistance = followDistance_; next.teleportDistance = teleportDistance_;
    next.pending = batch_.size()+spawnJobs_.size(); next.claimed = claims_.size();
    { std::scoped_lock lock(queueMutex_); next.pending+=queue_.size(); }
    next.autoFight=autoFight_; next.hardControl=hardControl_; next.isolateFactions=isolateFactions_;
    next.battleRadius=battleRadius_; next.focusEnemy=focusEnemy_; next.suppressed=removed_.size();
    next.spawnTotal=spawnTotal_; next.spawnDone=spawnDone_; next.spawnCreated=spawnCreated_; next.spawnFailed=spawnFailed_;
    for(const auto& job:spawnJobs_) next.spawnPending+=static_cast<std::size_t>(job.total-job.done);
    for(const auto& [id,entry]:spawned_) next.spawned.push_back({id,entry.name,false,0,{}});
    std::sort(next.spawned.begin(),next.spawned.end(),[](const NamedRef& a,const NamedRef& b){return a.id<b.id;});
    next.runtime10=runtimeReady10_;
    if(auto* q=runtime10::Form<RE::TESQuest>(runtime10::Quest))next.quest10=q->IsEnabled() && q->IsRunning();
    next.aura10=aura10_;next.emptyWorld10=emptyWorld10_;next.protectArmy10=protectArmy10_;
    next.purgeBusy10=!clearJobs10_.empty();next.clearRadius10=clearRadius10_;next.stuckDelay10=stuckDelay10_;
    next.manifest10=manifest10_;next.resolved10=resolved10_;next.dc10=dc10_;
    next.spawnFollowReady14=spawnFollowReady14_;next.spawnFollowPending14=spawnFollowPending14_;
    next.spawnVisible10=spawnVisible10_;next.births10=births10_.size();next.spawnPending+=births10_.size();
    next.clearDone10=clearDone10_;next.clearTotal10=clearTotal10_;next.clearPending10=clearJobs10_.size();
    next.catalogStatus10=catalogStatus10_;next.travel10=travel10_;next.keepTravel10=keepTravel10_;
    next.pauseMenu=true;
    // Report the engine's actual full-pause readback, not only freezeTime.
    if (auto* ui=RE::UI::GetSingleton()) next.worldPaused=ui->GameIsPaused();
    RefreshSexLab16(next);
    next.closing=closeRequested_;
    next.superHealth=healthBoost_.enabled; next.infiniteMagicka=magickaBoost_.enabled;
    next.forcedFollow=forcedFollow_; next.fontScale=fontScale_;
    next.batchTotal=batchTotal_; next.batchDone=batchDone_; next.batchSent=batchSent_;
    next.batchSkipped=batchSkipped_; next.batchFailed=batchFailed_;
    next.inspected=inspected_; next.inspectedCount=inspectedCount_; next.inspectedKnown=inspectedKnown_; next.inspectedAddable=inspectedAddable_;
    next.personalSlot11=lastPersonalSlot11_;next.personalRequest11=lastPersonalRequest11_;
    next.rules11=rules11_;next.law11=law11::Snapshot();
    next.rejectedDialogues14=rejectedDialogues14_;next.releasedScenes14=releasedScenes14_;
    next.controlsRestored11=controlsRestored11_;next.guardReleases11=guardReleases11_;next.activationRepairs11=activationRepairs11_;next.questStops11=questStops11_;
    next.protectedObjects11=ownRules11_.size();
    next.suspended11=std::max(0.0f,std::chrono::duration<float>(suspendUntil11_-Clock::now()).count());
    next.quests11=questRows11_;next.quest11=questDetail11_;
    next.personalRuntime11=runtime10::Form<RE::TESQuest>(kPersonalQuestFirst11)!=nullptr;
    for(const auto& q:personal11_)if(q)next.personal11.push_back(*q);
    next.messages.assign(notes_.begin(), notes_.end());
    auto* player = RE::PlayerCharacter::GetSingleton();
    if (ready_ && player) {
        next.playerPosition11=Point(player->GetPosition());if(auto* world=player->GetWorldspace())next.playerWorld11=world->GetFormID();
        next.health=player->GetActorValue(RE::ActorValue::kHealth); next.maxHealth=player->GetActorValueMax(RE::ActorValue::kHealth);
        next.magicka=player->GetActorValue(RE::ActorValue::kMagicka); next.maxMagicka=player->GetActorValueMax(RE::ActorValue::kMagicka);
        next.stamina12=player->GetActorValue(RE::ActorValue::kStamina); next.maxStamina12=player->GetActorValueMax(RE::ActorValue::kStamina);
        next.level12=player->GetLevel(); next.carry12=player->GetActorValue(RE::ActorValue::kCarryWeight);
        next.regen12={player->GetBaseActorValue(RE::ActorValue::kHealRate),player->GetBaseActorValue(RE::ActorValue::kMagickaRate),player->GetBaseActorValue(RE::ActorValue::kStaminaRate)};
        next.speedMult=player->GetActorValue(RE::ActorValue::kSpeedMult); next.runSpeed=player->GetRunSpeed();
        if (auto* cell = player->GetParentCell()) { next.playerCell = cell->GetFormID(); next.cellName = Name(cell); }
        if (auto* ref = Ref(selected_)) {
            auto& t = next.target; t.id = selected_; t.name = Name(ref);
            if (auto* base = ref->GetBaseObject()) t.base = base->GetFormID();
            if (auto* cell = ref->GetParentCell()) { t.cell = cell->GetFormID(); t.cellName = Name(cell); t.interior = cell->IsInteriorCell(); }
            auto* owner=ref->GetOwner();
            if (!owner && ref->GetParentCell()) owner=ref->GetParentCell()->GetOwner();
            if (owner) t.owner=owner->GetFormID();
            t.locked=ref->IsLocked();t.activationBlocked=ref->IsActivationBlocked();
            t.owned=ref->IsAnOwner(player,true,false);
            auto* extra=ref->extraList.GetByType<RE::ExtraTeleport>();
            auto linked=extra && extra->teleportData ? extra->teleportData->linkedDoor.get() : RE::NiPointer<RE::TESObjectREFR>{};
            if (linked && linked->GetParentCell() && linked->GetParentCell()->IsInteriorCell()) {
                auto* cell=linked->GetParentCell(); t.house=cell->GetFormID(); t.houseName=Name(cell); t.houseOwned=claims_.contains(t.house);
            }
            t.position = Point(ref->GetPosition()); t.angle = Point(ref->GetAngle()); t.scale = ref->GetScale();
            t.disabled = ref->IsDisabled(); t.dragon = ref->IsDragon();
            if (auto* actor = ref->As<RE::Actor>()) {
                t.actor = true; t.dead = actor->IsDead(false); t.ai=actor->IsAIEnabled(); t.combat=actor->IsInCombat(); t.pacified=pacified_.contains(selected_);
                t.health = actor->GetActorValue(RE::ActorValue::kHealth);
                t.magicka = actor->GetActorValue(RE::ActorValue::kMagicka);
                t.stamina = actor->GetActorValue(RE::ActorValue::kStamina);
                t.essential=actor->IsEssential(); t.protectedActor=actor->IsProtected();
                t.invulnerable=actor->GetActorBase() && actor->GetActorBase()->IsInvulnerable();
                t.teammate=actor->IsPlayerTeammate(); t.sitting=actor->GetSitSleepState()!=RE::SIT_SLEEP_STATE::kNormal;
                t.restrained=actor->GetLifeState()==RE::ACTOR_LIFE_STATE::kRestrained;
                if(auto* package=actor->GetCurrentPackage()) { t.packageID=package->GetFormID(); t.packageName=Name(package); }
                if(auto* scene=actor->GetCurrentScene()) t.sceneID=scene->GetFormID();
            }
        } else if (auto* form = RE::TESForm::LookupByID(selected_)) if (auto* quest = form->As<RE::TESQuest>()) {
            next.target.id = selected_; next.target.name = Name(quest); next.target.stage = quest->GetCurrentStageID();
        }
        if (menuOpen_) for (ID id : Actors()) if (auto* ref = Ref(id)) {
            if (ref->GetParentCell() == player->GetParentCell() || Distance(Point(ref->GetPosition()), Point(player->GetPosition())) < 10000)
                next.nearby.push_back({id,Name(ref),false,Distance(Point(ref->GetPosition()),Point(player->GetPosition())),{}});
        }
    }
    std::sort(next.nearby.begin(),next.nearby.end(),[](const NamedRef& a,const NamedRef& b) {
        if (a.distance!=b.distance) return a.distance<b.distance; return a.id<b.id;
    });
    for (const auto& [id,state]:followers_) {
        auto* ref=Ref(id);
        const float distance=ref && player?Distance(Point(ref->GetPosition()),Point(player->GetPosition())):0;
        next.followers.push_back({id,Name(ref)+" ["+Hex(id)+"]",state.waiting,distance,
            state.status+fmt::format(" | 拉回 {} 次",state.recoveries)});
    }
    std::sort(next.followers.begin(),next.followers.end(),[](const NamedRef& a,const NamedRef& b){return a.id<b.id;});
    for (const auto& b : bookmarks_) next.bookmarks.push_back({b.marker, b.name, false});
    { std::scoped_lock lock(viewMutex_); view_ = std::move(next); }
}
void Engine::RestoreTransient() {
    pauseTickPending15_=0; // Invalidates old-session UI callbacks without touching newer tickets.
    law11::SetPolicy(false,false);
    RestoreQuestTexts11();
    menuOpen_ = false; closeRequested_=false; closeFinalization15_=false; SyncInput();
    // CELL names are runtime base-form edits. Undo only our previous-session name before another save loads.
    for (const auto& [id,name]:renamedCells_) if (auto* c=RE::TESForm::LookupByID<RE::TESObjectCELL>(id)) c->SetFullName(name.c_str());
    if (!renamedCells_.empty()) {
        std::scoped_lock lock(catalogMutex_);
        if (catalog_) {
            auto changed=std::make_shared<std::vector<Entry>>(*catalog_);
            for (auto& entry:*changed) if (auto it=renamedCells_.find(entry.id);it!=renamedCells_.end()) {
                entry.name=it->second; entry.search=entry.name+" "+entry.editor+" "+entry.source+" "+Hex(entry.id);
            }
            catalog_=std::move(changed);
        }
    }
    renamedCells_.clear(); afterClose_.clear(); CancelActorSpawns(); deathJobs_.clear();
    if (detectionCaptured_) if (auto* p = RE::ProcessLists::GetSingleton()) p->runDetection = detectionBefore_;
    detectionCaptured_ = false;
    for (const auto& [id,enabled] : frozen_) if (auto* ref = Ref(id)) if (auto* actor = ref->As<RE::Actor>()) actor->EnableAI(enabled);
    frozen_.clear();
    if (flight_) Run("tcl");
    flight_ = false;
}
void Engine::ResetSession() {
    pauseTickPending15_=0;
    ResetKernel11();
    ready_ = false; menuOpen_ = false; ++epoch_;
    ResetRuntime10(); keepTravel10_=false;travel10_.clear();
    sexLabActors16_.clear(); sexLabStatus16_="尚未检查 SexLab。"; audioAvailable16_=audioMuted16_=audioWarned16_=false;
    spawnJobs_.clear(); spawned_.clear(); removed_.clear(); deathJobs_.clear();
    spawnTotal_=spawnDone_=spawnCreated_=spawnFailed_=0; focusEnemy_=0; removalCursor_=0; removalTimer_=0;
    autoFight_=defaultAutoFight_; hardControl_=defaultHardControl_; isolateFactions_=defaultIsolateFactions_; battleRadius_=defaultBattleRadius_;
    { std::scoped_lock lock(queueMutex_); queue_.clear(); }
    batch_.clear(); afterClose_.clear(); pacified_.clear(); renamedCells_.clear(); healthBoost_={}; magickaBoost_={};
    closeRequested_=false; batchTotal_=batchDone_=batchSent_=batchSkipped_=batchFailed_=0; inspected_=0;
    followers_.clear(); claims_.clear(); bookmarks_.clear(); frozen_.clear(); claimedThisVisit_.clear();
    selected_ = lastMarker_ = 0;
    peace_ = ignore_ = freeze_ = flight_ = unlimited_ = false;
    detectionCaptured_ = false;
}
nlohmann::json Engine::PersistentState() const {
    nlohmann::json j;
    j["freedom10"]={{"radius",clearRadius10_},{"aura",aura10_},{"emptyWorld",emptyWorld10_},{"protectArmy",protectArmy10_},{"stuckDelay",stuckDelay10_},{"keepTravel",keepTravel10_}};
    j["freedom10"]["enemies"]=nlohmann::json::array();for(ID id:enemies10_)j["freedom10"]["enemies"].push_back(id);
    j["flags"] = {peace_,ignore_,freeze_,unlimited_};
    j["last"] = lastMarker_;
    j["settings"] = {followDistance_,teleportDistance_,flightSpeed_};
    j["dragonsIndoors"] = dragonsIndoors_;
    j["realtime7"]={ {"pauseMenu",true},{"forcedFollow",forcedFollow_},{"fontScale",fontScale_},
        {"health",{healthBoost_.enabled,healthBoost_.original,healthBoost_.applied}},
        {"magicka",{magickaBoost_.enabled,magickaBoost_.original,magickaBoost_.applied}} };
    j["pacified"]=nlohmann::json::array();
    for (const auto& [id,old]:pacified_) j["pacified"].push_back({id,old});
    j["followers"] = nlohmann::json::array();
    for (const auto& [id,s] : followers_) j["followers"].push_back({id,s.waiting,s.oldAI,s.oldTeammate,s.oldAggression,s.oldWaiting});
    auto& legion=j["legion9"];
    legion={{"autoFight",autoFight_},{"hardControl",hardControl_},{"isolateFactions",isolateFactions_},
        {"battleRadius",battleRadius_},{"focus",focusEnemy_}};
    legion["members"]=nlohmann::json::array();
    for(const auto& [id,state]:followers_) {
        auto profile=state.profile; profile.Normalize();
        nlohmann::json member={{"id",id},{"initialized",state.initialized9},{"isolated",state.isolated},
            {"controlsCaptured",state.controlsCaptured},{"oldFlags",state.oldControlFlags},{"oldBits",state.oldControlBits},
            {"oldConfidence",state.oldConfidence},{"oldAssistance",state.oldAssistance},{"profileEnabled",state.profileEnabled},
            {"profile",{profile.health,profile.magicka,profile.stamina,profile.damage,profile.armor,profile.resist,profile.speed,profile.refill}}};
        member["factions"]=nlohmann::json::array();
        for(const auto& [faction,rank]:state.oldFactions) member["factions"].push_back({faction,rank});
        legion["members"].push_back(std::move(member));
    }
    legion["removed"]=nlohmann::json::array(); legion["spawned"]=nlohmann::json::array();
    for(const auto& [id,entry]:removed_) legion["removed"].push_back({id,entry.base,entry.name.substr(0,128)});
    for(const auto& [id,entry]:spawned_) legion["spawned"].push_back({id,entry.base,entry.name.substr(0,128)});
    j["claims"] = nlohmann::json::array();
    for (const auto& [id,c] : claims_) j["claims"].push_back({id,c.eviction,c.exit,c.name});
    j["bookmarks"] = nlohmann::json::array();
    for (const auto& b : bookmarks_) j["bookmarks"].push_back({b.marker,b.name});
    j["frozen"] = nlohmann::json::array();
    for (const auto& [id,enabled] : frozen_) j["frozen"].push_back({id,enabled});
    SaveKernel11(j);
    return j;
}
void Engine::RestoreState(const nlohmann::json& j, SKSE::SerializationInterface* serial) {
    auto resolve = [&](const nlohmann::json& value) -> ID {
        ID old = value.get<ID>(), out = 0;
        return old && serial->ResolveFormID(old,out) ? out : 0;
    };
    LoadKernel11(j,serial);
    aliasReconciled10_=false;
    if(j.contains("freedom10")) {
        const auto& v=j.at("freedom10");clearRadius10_=Finite(v.value("radius",2000.0f),2000,1,100000);
        aura10_=v.value("aura",false);emptyWorld10_=v.value("emptyWorld",false);protectArmy10_=v.value("protectArmy",true);
        stuckDelay10_=Finite(v.value("stuckDelay",12.0f),12,5,120);keepTravel10_=v.value("keepTravel",false);
        if(v.contains("enemies"))for(const auto& x:v.at("enemies")){if(enemies10_.size()>=10000)break;const ID id=resolve(x);if(id && id!=0x14)enemies10_.insert(id);}
    }
    const auto& flags = j.at("flags");
    peace_ = flags.at(0).get<bool>(); ignore_ = flags.at(1).get<bool>();
    freeze_ = flags.at(2).get<bool>(); unlimited_ = flags.at(3).get<bool>();
    lastMarker_ = resolve(j.at("last"));
    const auto& settings = j.at("settings");
    followDistance_ = Finite(settings.at(0).get<float>(),240,64,10000);
    // 0.1.x default was a very wide 600-unit deadzone. Migrate the default, not custom distances.
    if (!j.contains("realtime7") && followDistance_==600) followDistance_=240;
    teleportDistance_ = Finite(settings.at(1).get<float>(),4000,200,100000);
    flightSpeed_ = Finite(settings.at(2).get<float>(),1000,10,20000);
    dragonsIndoors_ = j.value("dragonsIndoors",false);
    for (const auto& x : j.at("followers")) {
        if (followers_.size() >= 512) break;
        auto id = resolve(x.at(0));
        if (id) {
            FollowState s; s.id=id; s.waiting=x.at(1).get<bool>(); s.oldAI=x.at(2).get<bool>(); s.oldTeammate=x.at(3).get<bool>();
            s.oldAggression=Finite(x.at(4).get<float>(),0,0,3); s.oldWaiting=x.size()>5?Finite(x.at(5).get<float>(),0,0,1):0;
            followers_[id]=std::move(s);
        }
    }
    if(j.contains("legion9")) {
        const auto& legion=j.at("legion9");
        autoFight_=legion.value("autoFight",true); hardControl_=legion.value("hardControl",true);
        isolateFactions_=legion.value("isolateFactions",true);
        battleRadius_=Finite(legion.value("battleRadius",5000.0f),5000,256,20000);
        if(legion.contains("focus")) focusEnemy_=resolve(legion.at("focus"));
        if(legion.contains("members")) for(const auto& member:legion.at("members")) {
            const ID id=resolve(member.at("id")); auto it=followers_.find(id); if(it==followers_.end()) continue;
            auto& state=it->second;
            state.initialized9=member.value("initialized",false); state.isolated=member.value("isolated",false);
            state.controlsCaptured=member.value("controlsCaptured",false);
            state.oldControlFlags=member.value("oldFlags",std::uint32_t{}); state.oldControlBits=member.value("oldBits",std::uint32_t{});
            state.oldConfidence=Finite(member.value("oldConfidence",2.0f),2,0,4);
            state.oldAssistance=Finite(member.value("oldAssistance",0.0f),0,0,2);
            state.profileEnabled=member.value("profileEnabled",false);
            if(member.contains("profile")) {
                const auto& p=member.at("profile");
                state.profile={p.at(0).get<float>(),p.at(1).get<float>(),p.at(2).get<float>(),p.at(3).get<float>(),
                    p.at(4).get<float>(),p.at(5).get<float>(),p.at(6).get<float>(),p.at(7).get<bool>()};
                state.profile.Normalize();
            }
            if(member.contains("factions")) for(const auto& x:member.at("factions")) {
                if(state.oldFactions.size()>=2048) break;
                const ID faction=resolve(x.at(0)); if(faction) state.oldFactions[faction]=std::clamp(x.at(1).get<int>(),-1,127);
            }
        }
        auto restoreRefs=[&](const char* key,std::unordered_map<ID,Removal>& refs) {
            if(!legion.contains(key)) return;
            for(const auto& x:legion.at(key)) {
                if(refs.size()>=100000) break;
                const ID ref=resolve(x.at(0)), base=resolve(x.at(1));
                if(ref && ref!=0x14 && base) refs[ref]={base,x.at(2).get<std::string>().substr(0,128)};
            }
        };
        restoreRefs("removed",removed_); restoreRefs("spawned",spawned_);
        // Tombstones win over stale co-save followers, never reactivate a removed reference.
        for(const auto& [id,entry]:removed_) { followers_.erase(id); spawned_.erase(id); }
    }

    for (const auto& x : j.at("claims")) {
        if (claims_.size() >= 2048) break;
        auto id = resolve(x.at(0)); if (id) claims_[id] = ClaimState{id,std::clamp(x.at(1).get<int>(),0,3),resolve(x.at(2)),x.size()>3?x.at(3).get<std::string>().substr(0,128):"keqing"};
    }
    for (const auto& x : j.at("bookmarks")) {
        if (bookmarks_.size() >= 256) break;
        auto id = resolve(x.at(0)); if (id) bookmarks_.push_back({id,x.at(1).get<std::string>().substr(0,128)});
    }
    if (j.contains("realtime7")) {
        const auto& rt=j.at("realtime7");
        pauseMenu_=true; forcedFollow_=rt.value("forcedFollow",false); // Ignore legacy pauseMenu=false.
        fontScale_=Finite(rt.value("fontScale",1.0f),1,0.8f,1.6f);
        auto readBoost=[&](const char* key,ResourceBoost& boost) {
            if (!rt.contains(key)) return; const auto& x=rt.at(key);
            boost={x.at(0).get<bool>(),Finite(x.at(1).get<float>(),100,-1.0e12f,1.0e12f),Finite(x.at(2).get<float>(),100,-1.0e12f,1.0e12f)};
        };
        readBoost("health",healthBoost_); readBoost("magicka",magickaBoost_);
    }
    if (j.contains("pacified")) for (const auto& x:j.at("pacified")) {
        if (pacified_.size()>=10000) break;
        auto id=resolve(x.at(0)); if (id) pacified_[id]=Finite(x.at(1).get<float>(),0,0,3);
    }
    if (j.contains("frozen")) for (const auto& x : j.at("frozen")) {
        if (frozen_.size() >= 10000) break;
        auto id = resolve(x.at(0)); if (id) frozen_[id] = x.at(1).get<bool>();
    }
}
void Engine::Save(SKSE::SerializationInterface* serial) {
    std::scoped_lock stateLock(Get().stateMutex_);
    try {
        auto text = Get().PersistentState().dump();
        if (text.size() > kMaxStateBytes || !serial->WriteRecord(kStateRecord,kRecordVersion,text.data(),static_cast<std::uint32_t>(text.size())))
            spdlog::error("State serialization failed.");
    } catch (const std::exception& e) { spdlog::error("Save callback: {}",e.what()); }
}
void Engine::Load(SKSE::SerializationInterface* serial) {
    std::scoped_lock stateLock(Get().stateMutex_);
    auto& e = Get();
    std::uint32_t type{},version{},length{};
    while (serial->GetNextRecordInfo(type,version,length)) {
        if (type != kStateRecord || version != kRecordVersion || length > kMaxStateBytes) continue;
        std::string text(length,'\0');
        if (serial->ReadRecordData(text.data(),length) != length) { spdlog::error("Short co-save record."); continue; }
        try { e.RestoreState(nlohmann::json::parse(text),serial); }
        catch (const std::exception& ex) { e.ResetSession(); spdlog::error("Invalid co-save state: {}",ex.what()); }
    }
}
void Engine::Revert(SKSE::SerializationInterface*) { std::scoped_lock stateLock(Get().stateMutex_); Get().RestoreTransient(); Get().ResetSession(); }
}
