#ifndef FC_LEGION_TEST_DOUBLE_HEADER
#define FC_LEGION_TEST_DOUBLE_HEADER
#pragma once
// TEST DOUBLE ONLY. These are not CommonLib headers or an engine ABI definition.
// Used to execute actual Legion.cpp policy/control branches on a host compiler.
#include <algorithm>
#include <cctype>
#include <cstring>
#include <strings.h>
#include <map>
#include <stdexcept>
#include <typeindex>
#define _stricmp strcasecmp
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <filesystem>
#include <fstream>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "fc/Core.hpp"
#include "fc/RuntimePolicy.hpp"
#include "fc/LegionPolicy.hpp"
constexpr int VK_F8=119;
#ifdef FC_JSON11_ADAPTER
#include "json11_adapter.hpp"
#else
namespace nlohmann {
// Syntax-only JSON test double. These tests do not validate JSON parsing semantics.
class json {
    std::vector<json> list_;
public:
    static json parse(std::istream&) { return {}; }
    const json& at(const char*)const{return *this;}
    template<class T> T get()const{return T{};}
    template<class T> T value(const char*,T fallback)const{return fallback;}
    bool contains(const char*)const{return false;}
    auto begin()const{return list_.begin();}auto end()const{return list_.end();}
};
}
#endif
#ifdef FC_REAL_FMT11
#define FMT_HEADER_ONLY
#include <fmt/format.h>
#else
namespace fmt { template<class... T> std::string format(std::string_view s,const T&...) {return std::string(s);} }
#endif
namespace spdlog { template<class... T> void info(const T&...){} template<class... T> void debug(const T&...){} template<class... T> void warn(const T&...){} template<class... T> void error(const T&...){} }
namespace SKSE {struct MessagingInterface { struct Message; }; class SerializationInterface {public: std::unordered_map<std::uint32_t,std::uint32_t> mapped; bool ResolveFormID(std::uint32_t old,std::uint32_t& out){auto i=mapped.find(old);out=i==mapped.end()?old:i->second;return out!=0;}};}
namespace RE {
using FormID=std::uint32_t;
template<class E> struct Flags {std::uint32_t bits{}; E get()const{return static_cast<E>(bits);} bool any(E f)const{return (bits&static_cast<std::uint32_t>(f))!=0;} void set(E f){bits|=static_cast<std::uint32_t>(f);} void reset(E f){bits&=~static_cast<std::uint32_t>(f);} };
struct NiPoint3 {float x{},y{},z{};};
template<class T> struct NiPointer { T* p{}; T* get()const{return p;} T* operator->()const{return p;} explicit operator bool()const{return p!=nullptr;} };
struct TESForm; struct TESObjectREFR; struct Actor;
struct TESFile{char fileName[260]{"Skyrim.esm"};};
inline int heldReadLocks{};
struct BSReadLockGuard{explicit BSReadLockGuard(int&){++heldReadLocks;}~BSReadLockGuard(){--heldReadLocks;}};
inline int registryLock{};
struct ExtraMapMarker{};
struct TESWorldSpace;
struct ExtraList {
    std::unordered_map<std::type_index,void*> extras;
    template<class T>T* GetByType(){auto it=extras.find(typeid(T));return it==extras.end()?nullptr:static_cast<T*>(it->second);}
};
struct ObjectRefHandle {TESObjectREFR* p{};NiPointer<TESObjectREFR> get()const{return {p};}};
struct ActorHandle {Actor* p{};NiPointer<Actor> get()const{return {p};}};
inline std::unordered_map<FormID,TESForm*> registry;
struct TESForm {
    FormID id{};std::string name,editor;bool deleted{};TESFile* file{};
    TESForm(FormID i=0):id(i){if(i)registry[i]=this;}
    virtual ~TESForm(){if(id && registry[id]==this)registry.erase(id);}
    template<class T> T* As(){return dynamic_cast<T*>(this);}
    template<class T=TESForm> static T* LookupByID(FormID i){auto it=registry.find(i);return it==registry.end()?nullptr:dynamic_cast<T*>(it->second);}
    FormID GetFormID()const{return id;} bool IsDeleted()const{return deleted;}
    TESFile* GetFile(int)const{return file;}
    const char* GetName()const{return name.c_str();} const char* GetFormEditorID()const{return editor.c_str();}
    void SetFullName(const char* value){name=value?value:"";}
    TESObjectREFR* AsReference();
    static auto GetAllForms(){return std::pair{&registry,std::ref(registryLock)};}
};
struct TESWorldSpace:TESForm{using TESForm::TESForm;};
inline std::vector<std::unique_ptr<TESForm>> allocated;
struct TESBoundObject:TESForm{using TESForm::TESForm;bool inventory{};bool IsInventoryObject()const{return inventory;}};
struct TESObjectDOOR:TESBoundObject{using TESBoundObject::TESBoundObject;};
struct TESObjectCONT:TESBoundObject{using TESBoundObject::TESBoundObject;};
struct TESFurniture:TESBoundObject{using TESBoundObject::TESBoundObject;};
struct TESObjectACTI:TESBoundObject{using TESBoundObject::TESBoundObject;};
struct ACTOR_BASE_DATA {
    enum class Flag:std::uint32_t{kEssential=2,kProtected=1<<11,kIsGhost=1<<29,kInvulnerable=1u<<31};
    Flags<Flag> actorBaseFlags;
};
struct TESNPC:TESBoundObject{using TESBoundObject::TESBoundObject;ACTOR_BASE_DATA actorData;};
struct TESLevCharacter:TESBoundObject{using TESBoundObject::TESBoundObject;};
struct TESFaction:TESForm{using TESForm::TESForm;int gold{},violent{};void SetCrimeGold(int v){gold=v;}void SetCrimeGoldViolent(int v){violent=v;}};
struct RACE_DATA { enum class Flag:std::uint32_t {kSwims=1<<6,kFlies=1<<7,kWalks=1<<8,kImmobile=1<<9};Flags<Flag> flags; };
struct TESRace:TESForm{using TESForm::TESForm;RACE_DATA data;};
struct TESGlobal:TESForm{using TESForm::TESForm;float value{};};
struct SpellItem:TESForm{using TESForm::TESForm;};
struct TESQuest;struct BGSScene:TESForm{using TESForm::TESForm;TESQuest* parentQuest{};};
struct TESObjectCELL:TESForm{using TESForm::TESForm;bool inside{true},attached{true};TESForm* owner{};TESForm* GetOwner(){return owner;}bool IsAttached(){return attached;}bool IsExteriorCell()const{return !inside;}bool IsInteriorCell()const{return inside;}};
struct TESPackage;
struct TESObjectREFR:TESForm {
    using TESForm::TESForm; TESBoundObject* base{}; TESObjectCELL* cell{}; TESWorldSpace* world{reinterpret_cast<TESWorldSpace*>(1)};ExtraList extraList;
    TESForm* owner{};bool locked{},blocked{},activateOK{true};int activations{};bool lastDefault{};
    enum ChangeFlags{kOwnershipExtra=1};void AddChange(ChangeFlags){}
    bool IsLocked()const{return locked;}bool IsActivationBlocked()const{return blocked;}void SetActivationBlocked(bool v){blocked=v;}
    TESForm* GetOwner(){return owner;}void SetOwner(TESForm* p){owner=p;}bool IsAnOwner(Actor* p,bool,bool);
    bool ActivateRef(TESObjectREFR*,std::uint8_t,TESBoundObject*,int,bool d){++activations;lastDefault=d;return activateOK;}
    NiPoint3 pos{},angle{};bool disabled{},collision{true},threeD{true};int moves{};
    const char* GetDisplayFullName()const{return name.c_str();}
    TESObjectCELL* GetParentCell()const{return cell;} TESWorldSpace* GetWorldspace()const{return world;}
    NiPoint3 GetPosition()const{return pos;} NiPoint3 GetAngle()const{return angle;}
    TESBoundObject* GetBaseObject()const{return base;}
    bool IsDisabled()const{return disabled;} void Enable(bool){disabled=false;} void Disable(){disabled=true;}
    virtual void SetDelete(bool v){if(heldReadLocks)throw std::runtime_error("mutation under read lock");deleted=v;}
    void SetCollision(bool v){collision=v;}
    void* Get3D()const{return threeD?reinterpret_cast<void*>(1):nullptr;}
    ObjectRefHandle GetHandle(){return {this};}
    void SetPosition(const NiPoint3& p){pos=p;}
    void MoveTo(TESObjectREFR* other){pos=other->pos;cell=other->cell;world=other->world;++moves;}
    NiPointer<TESObjectREFR> PlaceObjectAtMe(TESBoundObject* b,bool persistent);
};
enum class ActorValue{kWaitingForPlayer,kAggression,kConfidence,kAssistance,kHealth,kMagicka,kStamina,kAttackDamageMult,kDamageResist,kResistMagic,kSpeedMult};
enum class ACTOR_LIFE_STATE{kAlive,kDead,kRestrained,kUnconcious};
enum class SIT_SLEEP_STATE{kNormal,kIsSitting};
using BSFixedString=std::string;
struct AIProcess{};
struct Actor:TESObjectREFR {
    using TESObjectREFR::TESObjectREFR;
    enum class BOOL_FLAGS:std::uint32_t {kAngryWithPlayer=1<<11,kIsTrespassing=1<<12,kCrimeSearch=1<<24,kMovementBlocked=1<<27,kAttackingDisabled=1<<20,kCastingDisabled=1<<21,kScenePackage=1,kEssential=1<<18,kProtected=1<<19,kIsInKillMove=1<<14};
    enum class BOOL_BITS:std::uint32_t {kForceGreetingPlayer=1<<12,kAttackOnNextTheft=1<<15,kHeadingFixed=1<<4,kParalyzed=1u<<31};
    struct ChangeFlags{enum ChangeFlag{kLifeState=1<<10};};
    AIProcess process;
    struct ACTOR_RUNTIME_DATA{AIProcess* currentProcess{};Flags<BOOL_FLAGS> boolFlags;Flags<BOOL_BITS> boolBits;ActorHandle currentCombatTarget;} runtime;
    bool guard{},arrested{};int alarmsStopped{},dialoguesStopped{};bool IsGuard()const{return guard;}
    void StopAlarmOnActor(){++alarmsStopped;}void ClearArrested(){arrested=false;}
    std::unordered_map<TESBoundObject*,int> inventory;auto GetInventoryCounts(){return inventory;}
    bool ai{true},dead{},teammate{},dragon{},hostile{},combat{},refuseDeath{},rejectPackage{},rejectAliasPackage{},swimming{},midair{};
    TESRace* race{};int navmeshMoves{};bool lastTempPackage{},lastCreatedPackage{},lastAllowFurniture{};
    // Adversarial schema-driven fixture hooks. Defaults preserve older unit tests;
    // fixture tests below interpret the actual shipped ESP conditions and can
    // defer engine selection instead of making EvaluatePackage synchronous.
    std::function<bool(TESPackage*)> packageEligible15;
    std::function<TESPackage*()> chooseAliasPackage15;
    bool deferEvaluation15{};int resetEvaluations15{},interrupts15{};
    TESPackage* pendingPackage15{};
    int combatStops{},combatStarts{},getups{},kills{},packagePuts{},evaluations{};
    float alpha{1}; ACTOR_LIFE_STATE life{ACTOR_LIFE_STATE::kAlive};SIT_SLEEP_STATE sit{SIT_SLEEP_STATE::kNormal};
    BGSScene* scene{};TESPackage* currentPackage{};TESPackage* aliasPackage{};ObjectRefHandle furniture;
    std::unordered_map<ActorValue,float> values;
    std::unordered_map<TESFaction*,std::int8_t> factions;
    ACTOR_RUNTIME_DATA& GetActorRuntimeData(){return runtime;}
    bool IsDead(bool)const{return dead;}bool IsAIEnabled()const{return ai;}void EnableAI(bool v){ai=v;}
    bool IsPlayerTeammate()const{return teammate;}bool IsDragon()const{return dragon;}
    TESRace* GetRace()const{return race;}bool IsSwimming()const{return swimming;}bool IsInMidair()const{return midair;}
    float GetActorValue(ActorValue av){auto it=values.find(av);return it==values.end()?100.0f:it->second;}
    float GetBaseActorValue(ActorValue av){return GetActorValue(av);}float GetActorValueMax(ActorValue av){return GetActorValue(av);}
    void SetBaseActorValue(ActorValue av,float value){values[av]=value;}
    void RestoreActorValue(ActorValue av,float amount){values[av]=GetActorValue(av)+amount;}
    bool VisitFactions(std::function<bool(TESFaction*,std::int8_t)> f){for(auto [p,r]:factions)if(f(p,r))return true;return false;}
    void RemoveFromFaction(TESFaction* f){factions[f]=-1;}void AddToFaction(TESFaction* f,std::int8_t r){factions[f]=r;}
    ACTOR_LIFE_STATE GetLifeState()const{return life;}bool IsUnconscious()const{return life==ACTOR_LIFE_STATE::kUnconcious;}
    void SetLifeState(ACTOR_LIFE_STATE v){life=v;}BGSScene* GetCurrentScene()const{return scene;}void SetCurrentScene(BGSScene* s){scene=s;}
    void StopCurrentDialogue(){++dialoguesStopped;}void SetPlayerControls(bool enabled){ai=!enabled;}
    bool IsInCombat()const{return combat;}void StopCombat(){++combatStops;combat=false;runtime.currentCombatTarget={};}
    bool StartCombat(Actor* enemy,void* =nullptr){++combatStarts;combat=true;runtime.currentCombatTarget={enemy};return true;}
    bool IsHostileToActor(Actor*)const{return hostile;}
    void EndInterruptPackage(bool){++interrupts15;currentPackage=nullptr;pendingPackage15=nullptr;}
    TESPackage* GetCurrentPackage(){return currentPackage;}
    void PutCreatedPackage(TESPackage* p,bool temp,bool created,bool furnitureOK){
        ++packagePuts;lastTempPackage=temp;lastCreatedPackage=created;lastAllowFurniture=furnitureOK;
        if(!rejectPackage && (!packageEligible15 || packageEligible15(p)))currentPackage=p;
    }
    void EvaluatePackage(bool,bool reset){
        ++evaluations;if(reset)++resetEvaluations15;
        auto* chosen=chooseAliasPackage15?chooseAliasPackage15():aliasPackage;
        if(chosen && !rejectPackage && !rejectAliasPackage && (!packageEligible15 || packageEligible15(chosen))) {
            if(deferEvaluation15)pendingPackage15=chosen;else currentPackage=chosen;
        }
    }
    void CompleteEvaluation15(){if(pendingPackage15)currentPackage=pendingPackage15;pendingPackage15=nullptr;}
    ObjectRefHandle GetOccupiedFurniture(){return furniture;}SIT_SLEEP_STATE GetSitSleepState()const{return sit;}
    void InitiateGetUpPackage(){++getups;}
    void StopInteractingQuick(bool){furniture={};sit=SIT_SLEEP_STATE::kNormal;}
    bool DoSetSitSleepState(SIT_SLEEP_STATE s){sit=s;return true;}bool NotifyAnimationGraph(const BSFixedString&){return true;}
    void MoveToNearestNavmesh(){++navmeshMoves;}
    void SetPosition(const NiPoint3& p,bool){pos=p;}
    TESNPC* GetActorBase(){return base?base->As<TESNPC>():nullptr;}
    void InterruptCast(bool){}void SetAlpha(float a){alpha=a;}
    void KillImpl(Actor*,float,bool,bool){++kills;if(!refuseDeath){dead=true;life=ACTOR_LIFE_STATE::kDead;}}
    void KillImmediate(){++kills;if(!refuseDeath){dead=true;life=ACTOR_LIFE_STATE::kDead;}}
    void AddChange(ChangeFlags::ChangeFlag){}
    bool AddSpell(SpellItem*){return true;}
};
inline bool TESObjectREFR::IsAnOwner(Actor* p,bool,bool){return p&&p->GetActorBase()==owner;}
struct PlayerCharacter:Actor{
    using Actor::Actor;static inline PlayerCharacter* instance{};static PlayerCharacter* GetSingleton(){return instance;}
    struct PFlags{bool isLoading{},aiControlledToPos{},aiControlledFromPos{},aiControlledPackage{},goToJailQueued{},servingJailTime{},forceQuestTargetRepath{};} playerFlags;
    enum class ByCharGenFlag{kDisableSaving=1,kDisableWaiting=2};struct GameStats{Flags<ByCharGenFlag> byCharGenFlag;} stats;
    struct Info{int sleepSeconds{},skillTrainingsThisLevel{};} info;
    PFlags& GetPlayerFlags(){return playerFlags;}GameStats& GetGameStatsData(){return stats;}Info& GetInfoRuntimeData(){return info;}
    void SetAIDriven(bool v){playerFlags.aiControlledToPos=playerFlags.aiControlledFromPos=playerFlags.aiControlledPackage=v;}
    void AdvanceSleepWaitTick(){info.sleepSeconds=std::max(0,info.sleepSeconds-3600);}
    void StartWaiting(int hours){info.sleepSeconds=hours*3600;}
    int GetCrimeGoldValue(TESFaction* f){return f->gold+f->violent;}void ClearAllCrimeGold(TESFaction* f){f->gold=f->violent=0;}
};
struct ControlMap{std::uint32_t enabled{},stored{};static ControlMap* GetSingleton(){static ControlMap c;return &c;}void GetControlsState(std::uint32_t& a,std::uint32_t& b){a=enabled;b=stored;}void SetControlsState(std::uint32_t a,std::uint32_t b){enabled=a;stored=b;}};
struct UI{bool paused{},dialogue{},application{},modal{},item{},custom{};static UI* GetSingleton(){static UI u;return &u;}bool GameIsPaused(){return paused;}bool IsMenuOpen(const BSFixedString& name){return name=="Dialogue Menu"&&dialogue;}bool IsApplicationMenuOpen(){return application;}bool IsModalMenuOpen(){return modal;}bool IsItemMenuOpen(){return item;}bool IsCustomRendering(){return custom;}};
struct MenuTopicManager{ObjectRefHandle speaker;static MenuTopicManager* GetSingleton(){static MenuTopicManager topic;return &topic;}};
enum class UI_MESSAGE_TYPE{kHide};
struct UIMessageQueue{int hides{};BSFixedString lastMenu;static UIMessageQueue* GetSingleton(){static UIMessageQueue q;return &q;}void AddMessage(const BSFixedString& menu,UI_MESSAGE_TYPE type,void*){if(type==UI_MESSAGE_TYPE::kHide){++hides;lastMenu=menu;}}};
struct CrosshairPickData{ObjectRefHandle target;static CrosshairPickData* GetSingleton(){static CrosshairPickData c;return &c;}ObjectRefHandle GetActiveTarget(){return target;}};

struct ProcessLists {static inline ProcessLists* instance{};static ProcessLists* GetSingleton(){return instance;}void ClearCachedFactionFightReactions(){}void StopCombatAndAlarmOnActor(Actor* a,bool){a->StopCombat();}};
enum class PACKAGE_PROCEDURE_TYPE{kFollowWithoutEscort};
struct PackageTarget {std::int8_t targType{}; union Target{ObjectRefHandle handle;void* object;Target():object(nullptr){}~Target(){}}target;int value{};};
struct PACKAGE_DATA {
    enum class GeneralFlag:std::uint32_t{kCreated=1,kMustComplete=2,kAllowSwimming=4,kPreferredSpeed=8,kIgnoreCombat=16,kNoCombatAlert=32,kWeaponsUnequipped=64};
    enum class InterruptFlag:std::uint32_t{kWorldInteractions=1,kRandomConversations=2,kHellosToPlayer=4};
    enum class PreferredSpeed{kRun};
    Flags<GeneralFlag> packFlags;Flags<InterruptFlag> foBehaviorFlags;PreferredSpeed maxSpeed{};
};
struct PACK_SCHED_DATA{enum class DayOfWeek{kAny};int month{},date{},hour{},minute{},duration{};DayOfWeek dayOfWeek{};};
struct TESPackage:TESForm {
    PackageTarget* packTarg{};PACKAGE_DATA packData;struct Schedule{PACK_SCHED_DATA psData;}packSched;
    ~TESPackage(){if(packTarg){std::destroy_at(packTarg);std::free(packTarg);}}
    static TESPackage* CreatePackage(PACKAGE_PROCEDURE_TYPE){auto p=std::make_unique<TESPackage>();auto* out=p.get();allocated.push_back(std::move(p));return out;}
};
inline void* malloc(std::size_t n){return std::malloc(n);}
inline FormID nextID=0xff001000;
inline NiPointer<TESObjectREFR> TESObjectREFR::PlaceObjectAtMe(TESBoundObject* b,bool) {
    std::unique_ptr<TESObjectREFR> p;
    if(b->As<TESNPC>() || b->As<TESLevCharacter>()) p=std::make_unique<Actor>(nextID++);
    else p=std::make_unique<TESObjectREFR>(nextID++);
    p->base=b;p->cell=cell;p->world=world;p->pos=pos;p->angle=angle;
    if(auto* actor=p->As<Actor>()) actor->runtime.currentProcess=&actor->process;
    auto* out=p.get();allocated.push_back(std::move(p));return {out};
}
inline TESObjectREFR* TESForm::AsReference(){return dynamic_cast<TESObjectREFR*>(this);}
enum class QuestFlag:std::uint32_t{kEnabled=1,kCompleted=2,kActive=1<<11};
struct BGSQuestObjective{int index{};Flags<int> state;BSFixedString displayText;};
struct TESQuestStage{struct{std::uint16_t index{};}data;};
struct TESQuest:TESForm {
    using TESForm::TESForm;
    std::array<int,512> aliases{};
    std::array<ObjectRefHandle,512> refs{};
    struct {Flags<QuestFlag> flags;}data;bool starting{};int stage{};
    std::vector<BGSQuestObjective*> objectives;std::vector<TESQuestStage>* executedStages{};std::vector<TESQuestStage*>* waitingStages{};std::vector<BGSScene*> scenes;
    struct ChangeFlags{enum{kQuestFlags=1};};void AddChange(int){}
    bool IsActive()const{return data.flags.any(QuestFlag::kActive);}bool IsCompleted()const{return data.flags.any(QuestFlag::kCompleted);}bool IsStarting()const{return starting;}int GetCurrentStageID(){return stage;}
    void Reset(){stage=0;data.flags.bits=0;enabled=running=false;for(auto* o:objectives)o->state.bits=0;}
    bool enabled{},running{},rejectBinding{},rejectStart{};int starts{},bindings{};
    TESPackage* assignedPackage{};
    bool IsEnabled()const{return enabled;}bool IsRunning()const{return running;}
    bool Start(){++starts;if(!rejectStart)enabled=running=true;return !rejectStart;}
    ObjectRefHandle GetAliasedRef(std::uint32_t i)const{return i<refs.size()?refs[i]:ObjectRefHandle{};}
    void ForceRefIntoAlias(std::uint32_t i,TESObjectREFR* ref){
        ++bindings;if(rejectBinding || i>=refs.size())return;
        if(auto* old=refs[i].p?refs[i].p->As<Actor>():nullptr)old->aliasPackage=nullptr;
        refs[i]={ref};if(auto* a=ref?ref->As<Actor>():nullptr)a->aliasPackage=assignedPackage;
    }
};
struct TESDataHandler {
    static inline TESDataHandler* instance{};
    std::map<std::pair<std::string,FormID>,TESForm*> keys;
    static TESDataHandler* GetSingleton(){return instance;}
    template<class T>std::vector<T*> GetFormArray(){std::vector<T*> out;for(auto [i,f]:registry)if(auto* p=f->As<T>())out.push_back(p);return out;}
    template<class T>T* LookupForm(FormID id,std::string_view file){auto it=keys.find({std::string(file),id});return it==keys.end()?nullptr:it->second->As<T>();}
};
struct DoorTeleportData{ObjectRefHandle linkedDoor;NiPoint3 position{},rotation{};};
struct ExtraTeleport{DoorTeleportData* teleportData{};};
struct TESSpellCastEvent{NiPointer<TESObjectREFR> object;FormID spell{};};
struct TESObjectLoadedEvent{FormID formID{};bool loaded{};};
enum class BSEventNotifyControl{kContinue};
template<class T>struct BSTEventSource{};
template<class T>struct BSTEventSink{virtual BSEventNotifyControl ProcessEvent(const T*,BSTEventSource<T>*)=0;virtual ~BSTEventSink()=default;};
struct ScriptEventSourceHolder {static ScriptEventSourceHolder* GetSingleton(){static ScriptEventSourceHolder x;return &x;}template<class T>void AddEventSink(BSTEventSink<T>*){};};
inline void TestMove(TESObjectREFR* r,const ObjectRefHandle&,TESObjectCELL* cell,TESWorldSpace* world,const NiPoint3& pos,const NiPoint3& angle){r->cell=cell;r->world=world;r->pos=pos;r->angle=angle;++r->moves;}
}
namespace REL {
struct RelocationID{RelocationID(int,int){}};
template<class Fn>struct Relocation{Fn f{&RE::TestMove};explicit Relocation(RelocationID){}template<class... Args>auto operator()(Args&&... args){return f(std::forward<Args>(args)...);}};
}

#endif
