#pragma once
#include "PCH.h"
#include "fc/RuntimePolicy.hpp"
#include "fc/LegionPolicy.hpp"
#include "fc/FreedomPolicy.hpp"
#include "fc/KernelPolicy.hpp"
#include "fc/PlayerFreedomPolicy14.hpp"
#include "CrimeHooks11.hpp"
#include "fc/PlayerCustomization.hpp"

namespace fc {
enum class Kind : int {
    Armor, Weapon, Spell, Shout, Word, Perk, NPC, Ammo, Book, Potion,
    Scroll, Misc, Key, Ingredient, Container, Furniture, Weather, Quest, Cell,
    Faction, Race, Outfit, Package, SoulGem, Light, LeveledActor, Count
};
inline constexpr const char* KindNames[] = {
    "护甲 / 服装", "武器", "法术 / 能力", "龙吼", "龙语", "Perk",
    "NPC / 生物", "弹药", "书籍", "药水", "卷轴", "杂项物品", "钥匙", "材料",
    "容器", "家具", "天气", "任务", "Cell", "阵营", "种族", "套装", "AI Package", "灵魂石", "可携带光源", "等级生物列表"
};
static_assert(std::size(KindNames) == static_cast<std::size_t>(Kind::Count));
struct Entry {
    ID id{};
    Kind kind{};
    std::string name, editor, source, search;
    bool addable{}, playable{true};
    ID race{}; std::string raceName; bool unique{}, preset{}, leveled{};
    bool creaturePack{}, dc{}, raceMatch{}, soldier{}, scripted{}; std::string packTags;
};
struct TargetInfo {
    ID id{}, base{}, cell{}, owner{};
    std::string name, cellName;
    std::array<float, 3> position{}, angle{};
    float scale{1}, health{}, magicka{}, stamina{};
    int stage{-1};
    bool actor{}, disabled{}, dead{}, interior{}, dragon{}, owned{}, ai{}, combat{}, pacified{};
    ID house{}; std::string houseName; bool houseOwned{};
    bool locked{},activationBlocked{};
    bool essential{}, protectedActor{}, invulnerable{}, teammate{}, sitting{}, restrained{};
    ID packageID{}, sceneID{}; std::string packageName;
};
struct NamedRef { ID id{}; std::string name; bool waiting{}; float distance{}; std::string state; };
struct View {
    KernelRules11 rules11; law11::Stats law11;
    ID playerWorld11{};std::array<float,3> playerPosition11{};int personalSlot11{-1};std::uint64_t personalRequest11{};
    std::uint64_t controlsRestored11{},guardReleases11{},activationRepairs11{},questStops11{};
    std::uint64_t rejectedDialogues14{},releasedScenes14{};
    float suspended11{}; bool personalRuntime11{};
    std::shared_ptr<const std::vector<QuestRow11>> quests11; QuestDetail11 quest11;
    std::vector<PersonalQuest11> personal11; std::size_t protectedObjects11{};
    TargetInfo target;
    std::vector<NamedRef> nearby, followers, bookmarks;
    std::vector<std::string> messages;
    std::string cellName;
    ID playerCell{};
    std::uint64_t epoch{};
    bool ready{}, peace{}, ignore{}, freeze{}, flight{}, unlimited{}, dragonsIndoors{};
    bool pauseMenu{true}, worldPaused{}, closing{}, superHealth{}, infiniteMagicka{}, forcedFollow{};
    float health{}, maxHealth{}, magicka{}, maxMagicka{}, speedMult{}, runSpeed{}, fontScale{1};
    float stamina12{}, maxStamina12{}, carry12{}; int level12{1};
    std::array<float,3> regen12{};
    std::size_t batchTotal{}, batchDone{}, batchSent{}, batchSkipped{}, batchFailed{};
    ID inspected{}; int inspectedCount{}; bool inspectedKnown{}, inspectedAddable{};
    std::size_t pending{}, claimed{};
    float flightSpeed{1000}, followDistance{240}, teleportDistance{4000};
    bool autoFight{true}, hardControl{true}, isolateFactions{true}; float battleRadius{5000};
    ID focusEnemy{}; std::size_t suppressed{}, spawnTotal{}, spawnDone{}, spawnCreated{}, spawnFailed{}, spawnPending{};
    std::vector<NamedRef> spawned;
    bool runtime10{}, quest10{}, aura10{}, emptyWorld10{}, protectArmy10{true}, purgeBusy10{};
    float clearRadius10{2000}, stuckDelay10{12};
    std::size_t manifest10{}, resolved10{}, dc10{}, spawnVisible10{}, births10{}, clearDone10{}, clearTotal10{}, clearPending10{};
    std::size_t spawnFollowReady14{},spawnFollowPending14{};
    std::string catalogStatus10; bool keepTravel10{}; std::vector<NamedRef> travel10;
    bool audioMuted16{},audioAvailable16{},sexLabInstalled16{},sexLabBound16{};
    std::vector<NamedRef> sexLabActors16; std::string sexLabStatus16;
};
enum class Op {
    Select, SelectCrosshair, SelectPlayer, Command, Unlock, Lock, Own, Enable, Disable, Delete,
    Move, Rotate, Scale, Collision, Clone, ClaimDoor, ClaimCell, Evict, Follow, Wait, Dismiss,
    Summon, Goto, StopCombat, SetAI, Peace, Ignore, FreezeAI, Add, Remove, Spawn,
    Equip, AddAll, Skills, CancelBatch, SaveBookmark, GoBookmark, RemoveBookmark, Return,
    Weather, ReleaseWeather, QuestStage, QuestStart, QuestStop, QuestReset, QuestComplete,
    AddFaction, RemoveFaction, FactionRelation, ClearBounties, SetRace, Outfit, Rename,
    Settings, Flight, Unlimited, CloseUI, PauseMenu, SetValue, SuperHealth, InfiniteMagicka,
    Kill, Pacify, StopNearbyCombat, FollowMode, Inspect, UISettings,
    Banish, ForceControl, LegionOrder, LegionStats, LegionOptions, SpawnActors, CancelSpawn, ClearSpawned, RebuildCatalog, SpawnRandom10, SpawnSoldiers10, DoorTransit10, ClearPulse10, ClearWorld10, ClearConfig10, ClearCancel10, LearnPower10, CastPower10, RescuePlayer10, FreedomSettings10, RefreshTravel10, LoadedActor10,
    KernelRules11, KernelPreset11, KernelSuspend11, ClearCrime11, OwnUse11, ProtectObject11,
    QuestSelect11, QuestRefresh11, QuestTrack11, QuestBlock11, QuestObjective11, QuestText11,
    PersonalSave11, PersonalCommand11, PersonalGoal11, Rest11,
    ResourceMax12, ResourcePercent12, PlayerLevel12,
    SexLabAdd16, SexLabRemove16, SexLabMove16, SexLabClear16, SexLabStart16, SexLabStopAll16
};
struct Action {
    Op op{};
    ID target{}, form{};
    int count{1};
    std::array<float, 4> value{};
    std::string text;
    std::uint64_t epoch{};
    std::string source;
    bool afterClose{}, onlyMissing{true}, playableOnly{true};
    CombatProfile profile; bool recruit{}, applyProfile{};
    bool enemy{}, protectArmy{true}; int scope{}; std::uint64_t seed{};
    KernelRules11 rules11; PersonalQuest11 personal11;
};
class Engine {
public:
    static Engine& Get();
    void Initialize();
    void Message(SKSE::MessagingInterface::Message* message);
    void RequestTick();
    void Submit(Action action);
    void SetMenuOpen(bool open);
    bool MenuOpen() const { return menuOpen_.load(); }
    int Hotkey() const { return hotkey_; }
    View Snapshot() const;
    std::shared_ptr<const std::vector<Entry>> Catalog() const;
    static void Save(SKSE::SerializationInterface* serial);
    static void Load(SKSE::SerializationInterface* serial);
    static void Revert(SKSE::SerializationInterface* serial);
    void OnSpellCast10(ID caster,ID spell);
    void OnLoaded10(ID id);
    void RecordPlayerActivation14();
    void RecordPlayerDialogueClosed14() { dialogueGeneration14_.fetch_add(1,std::memory_order_relaxed); }
private:
    void ApplyPlayer12(const Action& a);
    void ApplySexLab16(const Action& a);
    bool DispatchSexLab16(const Action& a);
    bool DispatchSexLabStopAll16();
    void RefreshSexLab16(View& next);
    void SyncAudioFreeze16();
    void TickKernel11(float dt);
    bool TickPlayerFreedom14(bool dialogue);
    void MaintainKernel11(float dt,const std::vector<ID>& actors);
    void ApplyKernel11(const Action& a);
    void ClearCrime11(bool allFactions);
    void ProtectObject11(RE::TESObjectREFR* ref,bool unlock=true);
    void ResetKernel11();
    void RestoreQuestTexts11();
    void ApplyQuest11(const Action& a);
    void UpdateQuestView11();
    void MaintainQuests11(float dt);
    bool IsCoreQuest11(RE::TESQuest* q) const;
    int PersonalSlot11(RE::TESQuest* q) const;
    void SetQuestText11(RE::TESQuest* q,int objective,std::string text,bool remember);
    bool SyncPersonal11(PersonalQuest11& q,bool restart);
    void SaveKernel11(nlohmann::json& j) const;
    void LoadKernel11(const nlohmann::json& j,SKSE::SerializationInterface* serial);
    struct FollowState {
        ID id{}; bool waiting{}, oldAI{true}, oldTeammate{};
        float oldAggression{}, oldWaiting{};
        FollowProgress progress;
        float retrySeconds{}; std::uint32_t recoveries{};
        std::string status;
        RE::TESPackage* package{}; // Runtime-only; compare to current package before any use.
        GoalProgress goalProgress;
        bool initialized9{}, isolated{}, controlsCaptured{}, profileEnabled{};
        std::uint32_t oldControlFlags{}, oldControlBits{};
        float oldConfidence{}, oldAssistance{}, controlTimer{}, combatCooldown{}, furnitureSeconds{};
        std::unordered_map<ID,int> oldFactions;
        CombatProfile profile;
        int aliasSlot{-1}; float stuckSeconds10{};
        bool aliasRefreshPending15{}; // Runtime-only: one safe selection refresh after a new alias or distance-setting change.
        std::uint32_t packageAttempts14{}; float recoveryCooldown14{}, packageLogCooldown14{}; // Runtime-only recovery state.
    };
    struct ClaimState { ID cell{}; int eviction{}; ID exit{}; std::string name{"keqing"}; };
    struct ResourceBoost { bool enabled{}; float original{}, applied{}; };
    struct Bookmark { ID marker{}; std::string name; };
    struct Removal { ID base{}; std::string name; };
    struct DeathJob { ID base{}; int attempts{}; float timer{}; bool fallback{}; };
    struct SpawnJob {
        ID form{}, anchor{}; int total{}, done{}; float radius{}; bool recruit{}, applyProfile{};
        CombatProfile profile; std::uint64_t epoch{};
        std::vector<ID> forms; bool enemy{};
    };
    struct Birth10 { ID id{},base{}; bool recruit{},applyProfile{},enemy{}; CombatProfile profile; float age{}; std::uint64_t epoch{}; bool modelReady14{},profileApplied14{}; };
    struct ClearJob10 {ID id{},base{};std::uint64_t epoch{};};
    bool EnsureRuntime10();
    bool BindFollower10(RE::Actor* actor,FollowState& state);
    void ReleaseFollower10(ID id);
    void ResetRuntime10();
    void AnnotateCatalog10(std::vector<Entry>& entries);
    void ProcessBirths10(float dt);
    void ConfigureEnemy10(RE::Actor* actor);
    void QueueRandom10(const Action& a,bool soldiers);
    void QueueClear10(bool world);
    void ProcessClear10();
    void MaintainFreedom10(float dt,const std::vector<ID>& actors);
    void ApplyFreedom10(const Action& a);
    void RefreshTravel10();
    void DoorTransit10(RE::TESObjectREFR* door);
    void Tick();
    void Apply(const Action& a);
    void BuildCatalog();
    void RefreshView();
    void Maintain(float dt = 0.0f);
    void RefillResources();
    void SetBoost(ResourceBoost& state, RE::ActorValue av, bool enable, float limit);
    bool SetValue(RE::Actor* actor, const std::string& name, float value, bool effective);
    void RefreshHUD();
    void RefreshInspection();
    void FinishClose();
    void ConfigureClaim(const ClaimState& claim);
    void PlanBatch(const Action& a);
    void MaintainFlight(float dt);
    void SyncInput();
    void PumpPause15();
    void SyncNativePause15();
    bool NativePauseOpen15() const;
    bool NativePauseConfirmed15() const;
    bool OtherNativePause15() const;
    void Run(std::string_view text, RE::TESObjectREFR* target = nullptr);
    void Note(std::string text);
    RE::TESObjectREFR* Ref(ID id) const;
    void Own(RE::TESObjectREFR* ref);
    void Claim(RE::TESObjectCELL* cell, int eviction, ID exit);
    void ApplyClaim(const ClaimState& claim);
    void Evict(RE::Actor* actor, int mode, RE::TESObjectREFR* exit);
    void Follow(RE::Actor* actor);
    void WaitFollower(ID id,bool waiting);
    void Dismiss(ID id);
    void SetFollowPackage(RE::Actor* actor, FollowState& state);
    void PrepareLegionActor(RE::Actor* actor, FollowState& state, bool interrupt);
    void IsolateActorFactions(RE::Actor* actor, FollowState& state);
    void RestoreActorFactions(RE::Actor* actor, FollowState& state);
    void MaintainLegion(float dt, const std::vector<ID>& actors);
    bool IsLegionFriendly(RE::Actor* actor) const;
    void SetMemberStats(RE::Actor* actor, CombatProfile profile, bool refillNow);
    void ForceDeath(RE::Actor* actor, bool fallback);
    void DeathAttempt(RE::Actor* actor);
    void RemoveReference(RE::TESObjectREFR* ref);
    void MaintainRemovals(float dt);
    void QueueActorSpawn(const Action& action);
    void ProcessActorSpawns();
    void CancelActorSpawns();
    void ApplyLegionOrder(const Action& action);
    int AddForm(RE::TESForm* form, int count, bool remove, RE::Actor* receiver);
    ID MarkerAtPlayer(ID reuse = 0);
    void TeleportPlayer(RE::TESObjectREFR* target);
    void ResetSession();
    void RestoreTransient();
    nlohmann::json PersistentState() const;
    void RestoreState(const nlohmann::json& state, SKSE::SerializationInterface* serial);
    PlayerIntent14 playerIntent14_;
    std::atomic<std::uint64_t> dialogueGeneration14_{};
    std::uint64_t observedDialogueGeneration14_{};
    std::chrono::steady_clock::time_point freedomActionAt14_{};
    std::uint64_t rejectedDialogues14_{},releasedScenes14_{};
    bool crimeHooksAllowed11_{true};
    KernelRules11 rules11_{KernelRules11::Preset(1)};
    int lastPersonalSlot11_{-1};std::uint64_t lastPersonalRequest11_{};
    std::chrono::steady_clock::time_point suspendUntil11_{};
    std::uint64_t controlsRestored11_{},guardReleases11_{},activationRepairs11_{},questStops11_{};
    float kernelTimer11_{},questTimer11_{},sceneTimer11_{}; bool resting11_{},questDirty11_{true};
    std::unordered_map<ID,OwnRule11> ownRules11_; std::deque<ID> ownRing11_;
    std::unordered_set<ID> blockedQuests11_;
    ID questSelected11_{}; QuestDetail11 questDetail11_;
    std::shared_ptr<const std::vector<QuestRow11>> questRows11_;
    std::unordered_map<ID,QuestText11> questEdits11_,originalQuestText11_;
    std::array<std::optional<PersonalQuest11>,kPersonalQuestSlots11> personal11_;
    std::array<std::uint64_t,kPersonalQuestSlots11> syncedPersonal11_{};
    mutable std::recursive_mutex stateMutex_;
    mutable std::mutex queueMutex_, viewMutex_, catalogMutex_;
    std::deque<Action> queue_, batch_, afterClose_;
    std::shared_ptr<const std::vector<Entry>> catalog_;
    View view_;
    std::deque<std::string> notes_;
    std::atomic<bool> tickPending_{false}, menuOpen_{false}, ready_{false}, closeRequested_{false};
    std::atomic<std::uint64_t> pauseTickPending15_{0}, nextPauseTick15_{1};
    std::atomic<std::uint64_t> epoch_{1};
    ID selected_{}, lastMarker_{}, inspected_{};
    int inspectedCount_{}; bool inspectedKnown_{}, inspectedAddable_{};
    PauseLease pauseLease_;
    AudioMuteLease16 audioLease16_;
    bool audioAvailable16_{},audioMuted16_{},audioWarned16_{};
    std::vector<ID> sexLabActors16_;
    std::string sexLabStatus16_{"尚未检查 SexLab。"};
    RE::UI* pauseUI15_{}; // Registration owner only; never dereference a previous UI.
    std::optional<std::array<std::uint32_t,4>> nativePauseReadback15_;
    std::optional<bool> nativePauseLoggedRequest15_;
    std::chrono::steady_clock::time_point nativePauseLogAt15_{};
    bool closeFinalization15_{}; // Finish HUD/deferred work on the game task after a UI-only close.
    std::optional<bool> nativePauseRequest15_; // In-flight native show/hide until the next UI queue turn.
    ResourceBoost healthBoost_, magickaBoost_;
    std::unordered_map<ID,float> pacified_;
    std::unordered_map<ID,std::string> renamedCells_;
    bool pauseMenu_{true}, forcedFollow_{false}, dirty_{true}, hudDirty_{false};
    float fontScale_{1};
    int closeRefreshFrames_{};
    std::size_t batchTotal_{}, batchDone_{}, batchSent_{}, batchSkipped_{}, batchFailed_{};
    std::unordered_map<ID, FollowState> followers_;
    std::unordered_map<ID,Removal> removed_;
    std::unordered_map<ID,DeathJob> deathJobs_;
    std::unordered_map<ID,Removal> spawned_;
    std::deque<SpawnJob> spawnJobs_;
    std::size_t spawnTotal_{}, spawnDone_{}, spawnCreated_{}, spawnFailed_{};
    std::deque<Birth10> births10_;
    std::size_t spawnVisible10_{}, spawnFollowReady14_{}, spawnFollowPending14_{};
    AliasSlots10 slots10_; bool runtimeReady10_{}, aliasReconciled10_{};
    std::chrono::steady_clock::time_point questAttempt10_{};
    float stuckDelay10_{12}, clearRadius10_{2000}; bool aura10_{}, emptyWorld10_{}, protectArmy10_{true};
    std::deque<ClearJob10> clearJobs10_; std::unordered_set<ID> clearQueued10_;
    std::deque<ID> removalRing10_;
    std::unordered_set<ID> enemies10_;
    std::size_t clearTotal10_{}, clearDone10_{}, manifest10_{}, resolved10_{}, dc10_{};
    std::string catalogStatus10_;
     float freedomTimer10_{};
    std::vector<ID> requestedForms10_;
    bool bulkClear10_{},keepTravel10_{}; std::vector<NamedRef> travel10_;
    bool autoFight_{true}, hardControl_{true}, isolateFactions_{true};
    bool defaultAutoFight_{true}, defaultHardControl_{true}, defaultIsolateFactions_{true};
    float battleRadius_{5000}, defaultBattleRadius_{5000}; ID focusEnemy_{}; std::size_t removalCursor_{}; float removalTimer_{};
    std::unordered_map<ID, ClaimState> claims_;
    std::vector<Bookmark> bookmarks_;
    std::unordered_map<ID, bool> frozen_;
    std::unordered_set<ID> claimedThisVisit_;
    bool peace_{}, ignore_{}, freeze_{}, flight_{}, unlimited_{};
    bool inputBlocked_{}, detectionCaptured_{}, detectionBefore_{true};
    std::uint32_t controlsEnabled_{}, controlsStored_{};
    int hotkey_{VK_F8}, batchPerTick_{12};
    float followDistance_{240}, teleportDistance_{4000}, flightSpeed_{1000};
    bool dragonsIndoors_{false}, packageFollow_{true};
    std::chrono::steady_clock::time_point lastTick_{}, lastMaintain_{}, lastView_{};
};
}
