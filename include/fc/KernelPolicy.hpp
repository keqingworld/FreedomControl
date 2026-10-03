#pragma once
#include "fc/Core.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace fc {
inline constexpr std::uint32_t kKnownControls11 = 0x7FFu;
inline constexpr int kPersonalQuestSlots11 = 64;
inline constexpr int kPersonalGoalLimit11 = 8;
inline constexpr ID kPersonalQuestFirst11 = 0x900;

// Text is data, never executable console text. Bound UTF-8 by complete code points.
inline std::string CleanText11(std::string_view in, std::size_t limit) {
    std::string out; out.reserve(std::min(in.size(),limit));
    for (std::size_t i=0;i<in.size();) {
        const auto lead=static_cast<unsigned char>(in[i]);
        std::size_t n=lead<0x80?1:(lead>=0xC2&&lead<=0xDF?2:(lead>=0xE0&&lead<=0xEF?3:(lead>=0xF0&&lead<=0xF4?4:0)));
        bool valid=n && i+n<=in.size();
        for(std::size_t j=1;valid&&j<n;++j) valid=(static_cast<unsigned char>(in[i+j])&0xC0)==0x80;
        if(valid&&n==3) {const auto b=static_cast<unsigned char>(in[i+1]);valid=!(lead==0xE0&&b<0xA0)&&!(lead==0xED&&b>=0xA0);}
        if(valid&&n==4) {const auto b=static_cast<unsigned char>(in[i+1]);valid=!(lead==0xF0&&b<0x90)&&!(lead==0xF4&&b>=0x90);}
        if(!valid) {++i;continue;}
        if(lead<0x20 || lead==0x7F) {i+=n;continue;}
        if(out.size()+n>limit) break;
        out.append(in.substr(i,n)); i+=n;
    }
    return out;
}
struct KernelRules11 {
    bool noBounty{true},noArrest{true},guardTruce{true};
    bool controlGuard{},noAIDriven{},breakScenes{},breakFurniture{};
    bool playerFreedom14{true},preserveVoluntary14{true};
    bool aimedOwnership{},protectClaims{true},noSaveWaitLock{},unlimitedTraining{},keepFastTravel{};
    std::uint32_t controlMask{kKnownControls11};
    float guardRadius{4096};
    void Normalize() {controlMask&=kKnownControls11;guardRadius=Finite(guardRadius,4096,256,20000);}
    static KernelRules11 Preset(int p) {
        KernelRules11 r;
        if(p==0) {r.noBounty=r.noArrest=r.guardTruce=r.protectClaims=r.playerFreedom14=false;return r;}
        r.controlGuard=r.noAIDriven=r.aimedOwnership=r.noSaveWaitLock=r.keepFastTravel=true;
        if(p>=2) {r.breakScenes=r.breakFurniture=r.unlimitedTraining=true;}
        return r;
    }
};
inline std::uint32_t RestoreControls11(std::uint32_t existing,const KernelRules11& r,bool ownMenu,bool nativeMenu,bool suspended) {
    return r.controlGuard&&!ownMenu&&!nativeMenu&&!suspended ? existing|(r.controlMask&kKnownControls11) : existing;
}
inline bool GuardCandidate11(bool guard,bool dead,bool teammate,bool sameSpace,float distance,float radius) {
    return guard&&!dead&&!teammate&&sameSpace&&distance>=0&&distance<=radius;
}
enum class PersonalStatus11 : int { Draft, Running, Paused, Complete, Abandoned };
enum class GoalKind11 : int { Manual, CollectItem, ReachPoint, DefeatReference };
struct PersonalGoal11 {
    std::string text{"自定义目标"}; GoalKind11 kind{GoalKind11::Manual};
    ID form{},base{},world{}; std::array<float,3> position{}; int count{1}; float radius{256}; bool done{};
    void Normalize() {
        text=CleanText11(text,512); if(text.empty())text="自定义目标";
        if(static_cast<int>(kind)<0||static_cast<int>(kind)>3)kind=GoalKind11::Manual;
        count=std::clamp(count,1,1000000);radius=Finite(radius,256,32,20000);
        for(auto& p:position)p=Finite(p,0,-1.0e8f,1.0e8f);
    }
};
struct PersonalQuest11 {
    int slot{-1}; std::uint64_t revision{1};
    std::string title{"keqing 的自由任务"}; PersonalStatus11 status{PersonalStatus11::Draft};
    std::vector<PersonalGoal11> goals{PersonalGoal11{}};
    bool autoFinish{true};
    void Normalize() {
        title=CleanText11(title,240); if(title.empty())title="keqing 的自由任务";
        if(static_cast<int>(status)<0||static_cast<int>(status)>4)status=PersonalStatus11::Draft;
        if(goals.empty())goals.emplace_back();
        if(goals.size()>kPersonalGoalLimit11)goals.resize(kPersonalGoalLimit11);
        for(auto& g:goals)g.Normalize();
    }
};
inline bool AllGoalsDone11(const PersonalQuest11& q) {
    return !q.goals.empty()&&std::all_of(q.goals.begin(),q.goals.end(),[](const auto& g){return g.done;});
}
inline bool AutoGoal11(const PersonalGoal11& g,bool found,int itemCount,bool sameSpace,float distance,bool dead,bool matchingBase) {
    if(g.done)return true;
    if(!found || !g.form)return false;
    switch(g.kind) {
    case GoalKind11::CollectItem:return itemCount>=g.count;
    case GoalKind11::ReachPoint:return sameSpace&&distance>=0&&distance<=g.radius;
    case GoalKind11::DefeatReference:return matchingBase&&dead;
    default:return false;
    }
}
struct QuestRow11 {
    ID id{};std::string name,editor,source,search;
    bool enabled{},running{},active{},complete{},blocked{},protectedRuntime{},personal{}; int stage{},personalSlot{-1};
};
struct ObjectiveRow11 {int index{},state{};std::string text;};
struct QuestDetail11 {
    QuestRow11 row;std::vector<int> stages;std::vector<ObjectiveRow11> goals;bool valid{};std::string feedback;
};
struct OwnRule11 {ID base{};bool unlock{true};};
struct QuestText11 {std::string name;std::vector<std::pair<int,std::string>> goals;};
}
