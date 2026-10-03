#include "fc/KernelPolicy.hpp"
#include <iostream>
#include <stdexcept>
#include <limits>
#include <cmath>
int main(){
 int n=0;auto ck=[&](bool b){++n;if(!b)throw std::runtime_error("kernel policy #"+std::to_string(n));};using namespace fc;
 ck(CleanText11("abc",2)=="ab");ck(CleanText11("刻晴",3)=="刻");ck(CleanText11("刻晴",5)=="刻");ck(CleanText11("刻晴",6)=="刻晴");
 ck(CleanText11(std::string("a\0b\nc\td",7),99)=="abcd");ck(CleanText11("\xFF" "ab",10)=="ab");ck(CleanText11("\xC0\xAF",10).empty());ck(CleanText11("\xED\xA0\x80",10).empty());ck(CleanText11("\xF4\x90\x80\x80",10).empty());ck(CleanText11("\xF0\x9F\x8C\x99",4).size()==4);
 auto r=KernelRules11::Preset(1);ck(r.noBounty&&r.noArrest&&r.controlGuard&&r.aimedOwnership);ck(!r.breakScenes&&!r.breakFurniture);
 ck(!KernelRules11::Preset(0).noArrest);ck(KernelRules11::Preset(2).breakFurniture);
 ck(RestoreControls11(0x80000000,r,false,false,false)==0x800007ff);
 ck(RestoreControls11(0,r,true,false,false)==0);ck(RestoreControls11(0,r,false,true,false)==0);ck(RestoreControls11(0,r,false,false,true)==0);
 r.controlMask=0xffffffff;r.guardRadius=std::numeric_limits<float>::quiet_NaN();r.Normalize();ck(r.controlMask==0x7ff&&r.guardRadius==4096);
 r.controlMask=1;ck(RestoreControls11(2,r,false,false,false)==3);
 ck(GuardCandidate11(true,false,false,true,100,200));ck(!GuardCandidate11(false,false,false,true,100,200));ck(!GuardCandidate11(true,true,false,true,100,200));ck(!GuardCandidate11(true,false,true,true,100,200));ck(!GuardCandidate11(true,false,false,false,100,200));ck(!GuardCandidate11(true,false,false,true,201,200));ck(!GuardCandidate11(true,false,false,true,-1,200));
 PersonalGoal11 g;g.form=100;ck(!AutoGoal11(g,true,100,true,0,true,true));g.kind=GoalKind11::CollectItem;g.count=10;ck(!AutoGoal11(g,true,9,true,0,true,true));ck(AutoGoal11(g,true,10,false,999,false,false));ck(!AutoGoal11(g,false,10,true,0,true,true));
 g.kind=GoalKind11::DefeatReference;ck(!AutoGoal11(g,false,100,true,0,true,true));ck(!AutoGoal11(g,true,100,true,0,true,false));ck(!AutoGoal11(g,true,100,true,0,false,true));ck(AutoGoal11(g,true,100,true,0,true,true));g.form=0;ck(!AutoGoal11(g,true,100,true,0,true,true));
 g.form=10;g.kind=GoalKind11::ReachPoint;g.radius=100;ck(AutoGoal11(g,true,0,true,100,false,false));ck(!AutoGoal11(g,true,0,true,101,false,false));ck(!AutoGoal11(g,true,0,false,0,false,false));ck(!AutoGoal11(g,true,0,true,-1,false,false));
 g.kind=static_cast<GoalKind11>(999);g.count=-9;g.radius=-1;g.position[0]=std::numeric_limits<float>::infinity();g.Normalize();ck(g.kind==GoalKind11::Manual&&g.count==1&&g.radius>=32&&std::isfinite(g.position[0]));
 PersonalQuest11 q;ck(!AllGoalsDone11(q));q.goals[0].done=true;ck(AllGoalsDone11(q));q.goals.clear();ck(!AllGoalsDone11(q));q.Normalize();ck(q.goals.size()==1);q.goals.resize(99);q.Normalize();ck(q.goals.size()==8);q.status=static_cast<PersonalStatus11>(-1);q.Normalize();ck(q.status==PersonalStatus11::Draft);
 // Property loop is one test group, not 10,000 claimed gameplay cases.
 bool property=true;r=KernelRules11::Preset(1);for(unsigned v=0;v<10000;++v)property&=(RestoreControls11(v*30007,r,false,false,false)&~0x7ffu)==((v*30007)&~0x7ffu);ck(property);
 std::cout<<"PASS: "<<n<<" independent Kernel11 policy assertions. Not game tests.\n";
}
