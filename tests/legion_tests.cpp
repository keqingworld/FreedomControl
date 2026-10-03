#include "fc/LegionPolicy.hpp"
#include "fc/CreatureFollowPolicy.hpp"
#include <iostream>
#include <limits>
#include <set>
#include <stdexcept>
#include <vector>
int main() {
    int checks=0;
    auto check=[&](bool v) { ++checks; if(!v) throw std::runtime_error("Legion policy assertion #"+std::to_string(checks)); };
    using namespace fc;
    CombatProfile p; p.health=-1; p.speed=std::numeric_limits<float>::quiet_NaN(); p.damage=1e9f; p.Normalize();
    check(p.health==1); check(p.speed==100); check(p.damage==1000);
    for (int bits=0;bits<32;++bits) {
        const bool fight=LegionCanFight(bits&1,bits&2,bits&4,bits&8,bits&16);
        check(fight==(bits==1));
    }
    std::vector<CombatCandidate> c{{10,50,true},{11,10,true}};
    check(PickEnemy(c,1000)==11);
    c[0].attacksAlly=true; check(PickEnemy(c,1000)==10);
    c[1].explicitOrder=true; check(PickEnemy(c,1000)==11);
    c[1].friendly=true; check(PickEnemy(c,1000)==10);
    c[0].sameArea=false; check(PickEnemy(c,1000)==0);
    c={{0x14,10,true},{1,20,true,false,false,true},{2,30,true,false,false,false,true}};
    check(PickEnemy(c,1000)==0);
    c={{9,5,true},{8,5,true}}; check(PickEnemy(c,1000)==8);
    c={{9,std::numeric_limits<float>::quiet_NaN(),true}}; check(PickEnemy(c,1000)==0);
    std::set<std::pair<int,int>> slots;
    for(std::size_t i=0;i<MaxLegionMembers;++i) {
        const auto off=FormationOffset(i,0,240,160);
        check(IsFinitePoint(off)); check(slots.emplace(static_cast<int>(off[0]),static_cast<int>(off[1])).second);
    }
    for(int i=0;i<MaxSpawnBatch;++i) {
        const auto off=SpawnOffset(i,MaxSpawnBatch,400,0);
        check(IsFinitePoint(off)); check(Distance(off,{0,0,0})<=400.1f);
        check(SpawnOffset(i,MaxSpawnBatch,0,0)==std::array<float,3>{0,0,0});
    }
    ActorRecordFacts unnamed{false,false,false,0,"custom.esp","00001234"};
    ActorRecordFacts preset{false,false,true,7,"custom.esp","Preset"};
    ActorRecordFacts creature{false,false,false,8,"animals.esm","Wolf wolfRace 00101234"};
    ActorRecordFacts leveled{true,false,false,0,"level.esm","LCharWolf"};
    ActorRecordQuery query;
    for(const auto& e:{unnamed,preset,creature,leveled}) check(ActorRecordMatches(e,query));
    query.hidePresets=true; check(!ActorRecordMatches(preset,query)); check(ActorRecordMatches(unnamed,query));
    query={};query.type=1; check(!ActorRecordMatches(leveled,query)); check(ActorRecordMatches(creature,query));
    query={};query.type=2; check(ActorRecordMatches(leveled,query));check(!ActorRecordMatches(creature,query));
    query={};query.byRace=true;query.race=8;check(ActorRecordMatches(creature,query));check(!ActorRecordMatches(leveled,query));
    query={};query.source="custom.esp";check(ActorRecordMatches(unnamed,query));check(!ActorRecordMatches(creature,query));
    query={};query.query="WOLF";check(ActorRecordMatches(creature,query));check(!ActorRecordMatches(unnamed,query));
    GoalProgress progress;
    std::array<float,3> pos{0,0,0},goal{1000,0,0};
    check(!progress.Update(.5f,pos,goal,100,true,false,false,false).recover);
    bool recovered=false;
    for(int i=0;i<14;++i) { pos[0]-=20; auto d=progress.Update(.5f,pos,goal,100,true,false,false,false); recovered|=d.recover; }
    check(recovered); // walking the wrong way is not accepted as successful following
    progress.Reset(); pos={0,0,0}; goal={1000,0,0};
    for(int i=0;i<40;++i) { pos[0]+=20; goal[0]+=20; check(!progress.Update(.5f,pos,goal,100,true,false,false,false).recover); }
    progress.Reset();
    for(int i=0;i<40;++i) check(!progress.Update(.5f,{0,0,0},{1000,0,0},100,false,false,false,false).recover);
    check(progress.Update(.5f,{0,0,0},{1000,0,0},100,true,true,false,false).recover);
    check(progress.Update(.5f,{0,0,0},{1000,0,0},100,true,false,true,false).recover);
    check(progress.Update(.5f,{0,0,0},{1000,0,0},100,true,false,false,true).recover);
    check(!progress.Update(.5f,{950,0,0},{1000,0,0},100,true,false,false,true).recover);
    check(CheckDeath(true,0,true)==DeathDecision::Confirmed);
    check(CheckDeath(false,1,true)==DeathDecision::Retry);
    check(CheckDeath(false,3,true)==DeathDecision::Remove);
    check(CheckDeath(false,3,false)==DeathDecision::Failed);
    check(!MatchesRemoval(0x14,1,1)); check(!MatchesRemoval(22,1,2)); check(MatchesRemoval(22,1,1));
    check(!MatchesRemoval(22,0,0));
    // Silent creatures remain eligible: movement capabilities only, no dialogue gate.
    CreatureLocomotion14 ground{true,true,true,false,false,false};
    CreatureLocomotion14 aquatic{true,false,true,false,false,false};
    CreatureLocomotion14 flying{true,false,false,true,false,false};
    CreatureLocomotion14 fixed{true,true,false,false,true,false};
    check(ground.CanSnapToGround(false));check(!ground.CanSnapToGround(true));
    check(!aquatic.CanSnapToGround(false));check(!flying.CanSnapToGround(false));check(!fixed.CanSnapToGround(false));
    check(aquatic.AquaticOnly());check(!aquatic.CanRecoverNearPlayer(false,false));check(aquatic.CanRecoverNearPlayer(true,false));
    check(!ground.CanRecoverNearPlayer(false,true));check(flying.CanRecoverNearPlayer(false,true));
    CreatureLocomotion14 landOnly{true,true,false,false,false,false};
    check(!landOnly.CanRecoverNearPlayer(true,false));check(ground.CanRecoverNearPlayer(true,false));
    check(flying.CanRecoverNearPlayer(true,false));check(!fixed.CanRecoverNearPlayer(true,false));
    for(int flags=0;flags<64;++flags) {
        const bool allowed=CanForceFollowPackage14(flags&1,flags&2,flags&4,flags&8,flags&16,flags&32,2);
        check(allowed==(flags==17));
    }
    check(!CanForceFollowPackage14(true,false,false,false,true,false,1));
    check(NextFollowAttempt14(0)==1);check(NextFollowAttempt14(2)==3);check(NextFollowAttempt14(3)==3);
    std::cout << "PASS: " << checks << " independent Legion9 policy assertions (not Skyrim gameplay).\n";
}
