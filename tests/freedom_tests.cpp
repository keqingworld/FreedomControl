#include "fc/FreedomPolicy.hpp"
#include <iostream>
#include <stdexcept>
#include <set>

int main() {
    int checks=0;
    auto require=[&](bool ok){++checks;if(!ok)throw std::runtime_error("Freedom10 check "+std::to_string(checks));};
    require(fc::ParseSeed10("")==0);require(fc::ParseSeed10("0")==0);
    require(fc::ParseSeed10("18446744073709551615")==UINT64_MAX);
    for(auto bad:{"18446744073709551616","-1","+1","1.2","3abc"," 3","3 "})require(!fc::ParseSeed10(bad));
    fc::AliasSlots10 slots;
    require(slots.Acquire(0)<0);require(slots.Acquire(0x14)<0);
    for(std::uint32_t i=0;i<512;++i){require(slots.Acquire(0x1000+i)==static_cast<int>(i));require(slots.Acquire(0x1000+i)==static_cast<int>(i));}
    require(slots.Acquire(0x9000)<0);slots.Release(0x1000+33);require(slots.Acquire(0x9000)==33);
    require(slots.Find(0x1000+33)<0);slots.Reset();require(slots.Find(0x9000)<0);
    for(std::size_t i=0;i<8;++i)require(fc::FollowBand10(fc::FollowBands10[i])==i);
    require(fc::FollowBand10(NAN)==2);require(fc::FollowBand10(100000)==7);
    const std::vector<fc::ID> ids{0,3,2,2,1};auto a=fc::RandomBatch10(ids,256,42),b=fc::RandomBatch10(ids,256,42);
    require(a==b);require(a.size()==256);require(fc::RandomBatch10(ids,99999,1).size()==256);
    require(fc::RandomBatch10(ids,0,1).empty());require(fc::RandomBatch10(std::vector<fc::ID>{0},3,1).empty());
    std::array<int,3> counts{};fc::Random10 rng(234);
    for(int i=0;i<30000;++i)++counts[rng.Index(3)];
    for(int n:counts)require(n>9300 && n<10700);
    for(auto id:a)require(id>=1 && id<=3);
    fc::ClearFacts10 f{500,600,100,true,true,false,false};require(fc::ClearEligible10(f,200,false,true));
    f.id=0x14;require(!fc::ClearEligible10(f,200,true,false));f.id=500;
    f.friendly=true;require(!fc::ClearEligible10(f,200,true,true));require(fc::ClearEligible10(f,200,true,false));f.friendly=false;
    f.sameArea=false;require(!fc::ClearEligible10(f,200,false,false));require(fc::ClearEligible10(f,200,true,false));f.sameArea=true;
    f.deleted=true;require(!fc::ClearEligible10(f,200,true,false));f.deleted=false;
    f.distance=NAN;require(!fc::ClearEligible10(f,200,false,false));f.distance=-1;require(!fc::ClearEligible10(f,200,false,false));
    f.actor=false;require(!fc::ClearEligible10(f,200,true,false));f.actor=true;f.base=0;require(!fc::ClearEligible10(f,200,true,false));
    using B=fc::BirthResult10;
    require(fc::CheckBirth10(false,false,false,true,true,0)==B::Failed);
    require(fc::CheckBirth10(true,false,true,true,true,0)==B::Failed);
    require(fc::CheckBirth10(true,true,false,true,true,0)==B::Pending);
    require(fc::CheckBirth10(true,false,false,false,true,14)==B::Pending);
    require(fc::CheckBirth10(true,false,false,true,false,14)==B::Pending);
    require(fc::CheckBirth10(true,false,false,false,true,15)==B::Failed);
    require(fc::CheckBirth10(true,false,false,true,true,1)==B::Ready);
    fc::GoalProgress progress;fc::ProgressDecision decision;
    for(int i=0;i<47;++i){decision=progress.Update(.25f,{0,1000,0},{0,0,0},200,true,false,false,false,12);require(!decision.recover);}
    for(int i=0;i<3;++i)decision=progress.Update(.25f,{0,1000,0},{0,0,0},200,true,false,false,false,12);
    // Recovery can reset the policy after firing; verify at the explicit threshold separately.
    progress.Reset();bool recovered=false;for(int i=0;i<50;++i)recovered|=progress.Update(.25f,{0,1000,0},{0,0,0},200,true,false,false,false,12).recover;
    require(recovered);progress.Reset();
    for(int i=0;i<100;++i)require(!progress.Update(.25f,{0,1000.0f-static_cast<float>(i)*3,0},{0,0,0},200,true,false,false,false,12).recover);
    std::cout<<"PASS: "<<checks<<" portable Freedom10 assertions. Not engine/gameplay verification.\n";
}
