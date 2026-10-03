"""Execute production PlayerCustomization.cpp against explicit host-only engine doubles.
Not a Windows / CommonLib ABI / Skyrim gameplay test.
"""
from pathlib import Path
import subprocess,tempfile,shutil
R=Path(__file__).resolve().parents[1]
source=(R/'src/Engine.cpp').read_text()
apply=source.split('void Engine::Apply(const Action& a) {',1)[1]
assert apply.index('ApplyPlayer12(a)') < apply.index('if(a.op>=Op::KernelRules11)')
pch=(R/'tests/seam/PCH.h').read_text()
pch=pch.replace('struct AIProcess{};', '''struct CacheEntry {bool invalid{};};
struct Cache {std::array<CacheEntry,3> actorValueCache,maxActorValueCache;};
struct AIProcess{Cache data;Cache* cachedValues{&data};};
enum class ACTOR_VALUE_MODIFIER{kDamage};''')
a=pch.index('    float GetActorValue(ActorValue av)')
b=pch.index('    bool VisitFactions',a)
pch=pch[:a]+'''    std::unordered_map<ActorValue,float> effects,damage;
    int level{10}; int GetLevel(){return level;}
    float GetBaseActorValue(ActorValue av){auto i=values.find(av);return i==values.end()?100.0f:i->second;}
    float GetActorValueMax(ActorValue av){return GetBaseActorValue(av)+effects[av];}
    float GetActorValue(ActorValue av){return GetActorValueMax(av)+damage[av];}
    bool deathDuringBaseWrite{};
    void SetBaseActorValue(ActorValue av,float v){values[av]=v;if(av==ActorValue::kHealth && GetActorValue(av)<=0){dead=true;deathDuringBaseWrite=true;}}
    void ModActorValue(ACTOR_VALUE_MODIFIER,ActorValue av,float v){damage[av]+=v;}
    void RestoreActorValue(ActorValue av,float v){damage[av]+=v;}
'''+pch[b:]
with tempfile.TemporaryDirectory(prefix='fc-custom12-') as temp:
 t=Path(temp);(t/'PCH.h').write_text(pch)
 (t/'Engine.hpp').write_text((R/'src/Engine.hpp').read_text().replace('private:','public:'))
 for f in ('CrimeHooks11.hpp','PlayerCustomization.cpp'):shutil.copy2(R/'src'/f,t/f)
 (t/'test.cpp').write_text(r'''
#include "Engine.hpp"
#include <iostream>
#include <limits>
namespace fc {
int commandCount{};
void Engine::Note(std::string s){notes_.push_back(s);}
void Engine::Run(std::string_view,RE::TESObjectREFR*){++commandCount;}
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

}
int main(){
 using namespace fc;using AV=RE::ActorValue;int n=0;
 auto ck=[&](bool b){++n;if(!b)throw std::runtime_error("custom12 assertion "+std::to_string(n));};
 RE::PlayerCharacter p(0x14);RE::PlayerCharacter::instance=&p;p.runtime.currentProcess=&p.process;
 Engine e;auto send=[&](Op op,int index,float value,bool fill=false){Action a;a.op=op;a.count=index;a.value={value,fill?1.0f:0.0f,0,0};e.ApplyPlayer12(a);};
 p.values[AV::kHealth]=100;p.effects[AV::kHealth]=50;p.damage[AV::kHealth]=-75;
 send(Op::ResourceMax12,0,300);ck(p.GetActorValueMax(AV::kHealth)==300);ck(p.GetBaseActorValue(AV::kHealth)==250);ck(p.GetActorValue(AV::kHealth)==150);ck(p.effects[AV::kHealth]==50);
 send(Op::ResourceMax12,0,600,true);ck(p.GetActorValue(AV::kHealth)==600);
 send(Op::ResourceMax12,0,600,true);ck(p.GetBaseActorValue(AV::kHealth)==550);
 e.unlimited_=true;send(Op::ResourcePercent12,0,0);ck(p.GetActorValue(AV::kHealth)==1);ck(!e.unlimited_);
 send(Op::ResourcePercent12,0,50);ck(p.GetActorValue(AV::kHealth)==300);
 send(Op::ResourcePercent12,0,100);ck(p.GetActorValue(AV::kHealth)==600);
 send(Op::ResourcePercent12,1,0);ck(p.GetActorValue(AV::kMagicka)==0);
 send(Op::ResourceMax12,1,500,true);ck(p.GetActorValue(AV::kMagicka)==500);
 send(Op::ResourcePercent12,2,25);ck(p.GetActorValue(AV::kStamina)==25);
 send(Op::ResourceMax12,2,400);ck(p.GetActorValue(AV::kStamina)==100);
 for(float v:{-1.0f,0.0f,1000001.0f,std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()}){
  send(Op::ResourceMax12,0,v);ck(p.GetActorValueMax(AV::kHealth)==600);
 }
 for(float v:{-1.0f,101.0f,std::numeric_limits<float>::quiet_NaN()}){send(Op::ResourcePercent12,1,v);ck(p.GetActorValue(AV::kMagicka)==500);}
 send(Op::ResourceMax12,-1,200);send(Op::ResourceMax12,3,200);ck(p.GetActorValueMax(AV::kHealth)==600);
 p.dead=true;send(Op::ResourceMax12,0,100);ck(p.GetActorValueMax(AV::kHealth)==600);p.dead=false;
 e.healthBoost_={true,100,999950};p.values[AV::kHealth]=999950;p.damage[AV::kHealth]=0;
 send(Op::ResourceMax12,0,250,true);ck(!e.healthBoost_.enabled);ck(p.GetBaseActorValue(AV::kHealth)==200);ck(p.GetActorValueMax(AV::kHealth)==250);
 e.unlimited_=true;send(Op::ResourceMax12,0,1,true);ck(p.GetActorValue(AV::kHealth)==250);ck(p.GetBaseActorValue(AV::kHealth)==200);ck(e.unlimited_);
 p.effects[AV::kHealth]=0;send(Op::ResourceMax12,0,1,true);ck(p.GetActorValue(AV::kHealth)==1);ck(!p.deathDuringBaseWrite);
 for(auto& x:p.process.data.actorValueCache)ck(x.invalid);
 for(auto& x:p.process.data.maxActorValueCache)ck(x.invalid);
 send(Op::PlayerLevel12,0,0);send(Op::PlayerLevel12,65536,0);ck(commandCount==0);
 send(Op::PlayerLevel12,1,0);send(Op::PlayerLevel12,65535,0);ck(commandCount==2);

 p.values[AV::kHealth]=100;p.effects[AV::kHealth]=0;p.damage[AV::kHealth]=-50;e.unlimited_=true;
 send(Op::ResourceMax12,0,200,false);ck(p.GetActorValue(AV::kHealth)==100);ck(!e.unlimited_);
 e.RefillResources();ck(p.GetActorValue(AV::kHealth)==100);
 p.values[AV::kHealth]=100;p.effects[AV::kHealth]=500;p.damage[AV::kHealth]=0;e.unlimited_=true;
 send(Op::ResourceMax12,0,100,true);ck(p.GetBaseActorValue(AV::kHealth)==100);ck(e.unlimited_);
 p.values[AV::kMagicka]=0;p.effects[AV::kMagicka]=0;p.damage[AV::kMagicka]=0;e.unlimited_=true;
 send(Op::ResourcePercent12,1,50);ck(e.unlimited_);ck(p.GetActorValueMax(AV::kMagicka)==0);
 send(Op::ResourceMax12,1,100,true);ck(p.GetActorValueMax(AV::kMagicka)==100);ck(p.GetActorValue(AV::kMagicka)==100);
 p.values[AV::kHealth]=1000;p.effects[AV::kHealth]=0;p.damage[AV::kHealth]=-900;e.unlimited_=false;
 send(Op::ResourceMax12,0,100,false);ck(p.GetActorValue(AV::kHealth)==10);ck(!p.deathDuringBaseWrite);ck(!p.dead);
 e.healthBoost_={true,100,1000000};p.values[AV::kHealth]=1000000;p.damage[AV::kHealth]=-999950;
 send(Op::ResourcePercent12,0,50);ck(p.GetActorValueMax(AV::kHealth)==100);ck(p.GetActorValue(AV::kHealth)==50);ck(!e.healthBoost_.enabled);ck(!p.dead);
 p.values[AV::kHealth]=1;p.effects[AV::kHealth]=0;p.damage[AV::kHealth]=0;
 RE::PlayerCharacter::instance=nullptr;send(Op::ResourceMax12,0,200);ck(p.GetActorValueMax(AV::kHealth)==1);
 std::cout<<"PASS: "<<n<<" production custom12 assertions; explicit engine doubles only.\n";
}
''')
 subprocess.run(['g++','-std=c++23','-fsanitize=address,undefined','-fno-omit-frame-pointer','-I'+str(t),'-I'+str(R/'include'),str(t/'PlayerCustomization.cpp'),str(t/'test.cpp'),str(R/'src/Core.cpp'),'-o',str(t/'test')],check=True)
 subprocess.run([str(t/'test')],check=True)
