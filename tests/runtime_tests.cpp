#include "fc/RuntimePolicy.hpp"
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <random>
#include <vector>

int main() {
    unsigned checks=0;
    auto check=[&](bool condition,const char* label) {
        ++checks; if (!condition) { std::cerr << "FAIL: " << label << '\n'; std::exit(1); }
    };
    fc::PauseLease lease;
    check(!lease.Update(false,false).has_value(),"idle does not touch time");
    check(lease.Update(true,false)==true && lease.Held(),"acquire world pause");
    check(!lease.Update(true,true).has_value(),"do not recapture a pause we already own");
    check(lease.Update(false,true)==false && !lease.Held(),"restore original unpaused state");
    check(lease.Update(true,true)==true,"acquire while externally paused");
    check(lease.Update(false,true)==true,"external pause survives menu close");
    check(lease.Update(true,false)==true,"reopen");
    check(lease.Update(true,false)==true,"reassert lease when another writer clears it");
    check(lease.Update(false,true)==false,"reassert does not replace original state");
    fc::AudioMuteLease16 audio;
    check(!audio.Update(false,0.75f).has_value(),"idle audio lease does nothing");
    check(audio.Update(true,0.75f)==0.0f && audio.Held(),"capture master audio volume");
    check(!audio.Update(true,0.0f).has_value(),"muted audio stays untouched");
    check(audio.Update(true,0.5f)==0.0f,"reassert mute if another writer raises volume");
    check(audio.Update(false,0.0f)==0.75f && !audio.Held(),"restore exact previous master volume");
    check(audio.Update(true,0.0f)==0.0f,"capture pre-muted game");
    check(audio.Update(false,0.0f)==0.0f,"pre-muted game remains muted");
    for (int i=0;i<500;++i) {
        bool originally=(i%2)==0;
        check(lease.Update(true,originally)==true,"repeated acquisition");
        check(!lease.Update(true,true).has_value(),"lease remains stable");
        check(lease.Update(false,true)==originally,"repeated exact restoration");
    }
    check(!fc::NativePauseMessageNeeded15(false,false,{}),"idle native pause sends no message");
    check(fc::NativePauseMessageNeeded15(true,false,{}),"request native show");
    check(!fc::NativePauseMessageNeeded15(true,false,true),"no duplicate show before native readback");
    check(fc::NativePauseMessageNeeded15(false,false,true),"close cancels pending show even before it opens");
    check(fc::NativePauseMessageNeeded15(true,true,false),"reopen cancels pending hide");
    check(!fc::NativePauseMessageNeeded15(false,true,false),"no duplicate hide before native readback");
    check(fc::NativePauseMessageNeeded15(false,true,{}),"retry native hide after UI cycle readback");
    check(!fc::CanFinishClose(false,0,0),"no unsolicited close");
    check(!fc::CanFinishClose(true,1,0),"queued action precedes unpause");
    check(!fc::CanFinishClose(true,0,1),"batch item precedes unpause");
    check(fc::CanFinishClose(true,0,0),"close after drain");
    std::size_t left=101,done=0;
    while (left) {
        auto n=std::min<std::size_t>(12,left); left-=n; done+=n;
        check(fc::CanFinishClose(true,0,left)==(done==101),"multi-frame close barrier");
    }
    check(fc::MissingQuantity(20,7,true)==13,"top up stack");
    check(fc::MissingQuantity(1,5,true)==0,"skip owned gear");
    check(fc::MissingQuantity(20,7,false)==20,"explicit duplicate mode");
    check(fc::MissingQuantity(-100,-500,true)==1,"negative inputs clamped");
    check(fc::MissingQuantity(2000000000,0,true)==1000000,"quantity bounded");
    check(fc::MissingQuantity(100,2000000000,true)==0,"huge owned count does not overflow");
    std::mt19937 rng(20260925);
    for (int i=0;i<10000;++i) {
        const int want=static_cast<int>(rng()%2000)+1,have=static_cast<int>(rng()%4000);
        const int add=fc::MissingQuantity(want,have,true);
        check(add>=0 && have+add==std::max(want,have),"top-up invariant");
        check(fc::MissingQuantity(want,have+add,true)==0,"idempotent second bulk operation");
    }
    check(fc::DesiredBaseValue(100,100,200,true)==200,"effective speed without modifier");
    check(fc::DesiredBaseValue(120,100,200,true)==180,"preserve additive external modifier");
    check(fc::DesiredBaseValue(120,100,200,false)==200,"explicit base-value mode");
    check(fc::DesiredBaseValue(200,180,100,true)==80,"effective reset keeps modifier");
    check(fc::DesiredBaseValue(100,100,std::numeric_limits<float>::infinity(),true)==100,"invalid target refused");
    check(fc::DesiredBaseValue(100,100,100,true)==100,"idempotent stat edit");
    check(fc::BoostedBase(100,150,1000000)==999950,"boost retains temporary maximum contribution");
    check(fc::BoostedBase(2000000,2000000,1000000)==2000000,"do not lower existing super stat");
    check(fc::RestoreBoostedBase(999950,100,999950)==100,"boost removal exact");
    check(fc::RestoreBoostedBase(1000050,100,999950)==200,"external stat gain survives removal");
    check(fc::BoostedBase(-20,0,1000000)==999980,"negative baseline supported");
    const std::array<float,3> origin{0,0,0};
    fc::FollowProgress follow;
    auto step=[&](float distance,bool active=true,bool other=false,bool forced=false,float dt=0.25f) {
        return follow.Update(dt,origin,distance,240,4000,other,active,forced,2.5f);
    };
    check(!step(200).recover,"nearby follower should wait naturally");
    check(!step(700).recover,"prime progress before claiming stuck");
    for (int i=0;i<9;++i) check(!step(700).recover,"do not rescue too early");
    auto stuck=step(700); check(stuck.recover && stuck.stalled && !stuck.distant,"stuck rescue after simulated 2.5 seconds");
    check(step(5000).recover,"long-distance recovery");
    check(step(100,true,true).recover,"cross-cell recovery");
    check(!step(5000,false).recover,"no teleport while paused/waiting/frozen");
    check(follow.StalledSeconds()==0,"pause resets stuck accumulator");
    check(!step(700,true,false,true).recover,"forced mode primes current position");
    check(step(700,true,false,true).recover,"forced mode bypasses stuck waiting");
    follow.Reset();
    for (int i=0;i<20;++i) {
        std::array<float,3> moving{static_cast<float>(i)*20,0,0};
        check(!follow.Update(.25f,moving,1000,240,4000,false,true,false,2.5f).recover,"moving actor is not considered stuck");
    }
    follow.Reset();
    check(!step(700).recover,"reset tracking");
    check(!step(700,true,false,false,100000).recover,"load/menu gap does not cause instant rescue");
    check(follow.StalledSeconds()<=.75f,"elapsed time bounded");
    std::array<float,3> bad{std::numeric_limits<float>::quiet_NaN(),0,0};
    check(!follow.Update(.25f,bad,700,240,4000,false,true,false,2.5f).recover,"NaN position rejected");
    check(!step(std::numeric_limits<float>::infinity()).recover,"infinite distance rejected");
    for (const auto& size:std::vector<std::array<float,2>>{{320,240},{640,480},{1280,720},{1366,768},{1920,1080},{2560,1440},{3840,2160}}) {
        const auto b=fc::FitPanel(size[0],size[1]);
        check(b.left>=0 && b.top>=0 && b.width>0 && b.height>0,"positive centered panel");
        check(b.left+b.width<=size[0] && b.top+b.height<=size[1],"panel fits viewport");
        check(std::abs((size[0]-b.width)*.5f-b.left)<.01f,"center horizontally");
    }
    check(fc::IsFinitePoint(origin),"finite world point");
    check(!fc::IsFinitePoint(bad),"invalid world point");
    std::cout << "PASS: " << checks << " engine-independent policy assertions (includes deterministic property loops).\n";
}
