"""Compile regression for the user's COMPILE17 errors.

Checks use actual PCH order/audio body and the WHOLE launcher translation unit.
Windows/RE/Papyrus interfaces are explicit doubles, not real SDK/CommonLib.
Unpatched sources must fail for the logged symbols. No game DLL is generated.
"""
from __future__ import annotations
import argparse
from pathlib import Path
import shutil
import subprocess
import tempfile
import zipfile
import json
ROOT=Path(__file__).resolve().parents[1]
AUDIO_RE=r'''#pragma once
#include <cstdint>
namespace RE {
using FormID=std::uint32_t;
enum class DefaultObjectID { kMasterSoundCategory=145 };
struct BGSSoundCategory {
    float volume=0.65f; int writes=0;
    float GetCategoryVolume() const { return volume; }
    void SetCategoryVolume(float v) { volume=v; ++writes; }
};
struct BGSDefaultObjectManager {
    inline static BGSSoundCategory category{};
    inline static BGSSoundCategory* slot=&category;
    inline static bool present=true;
    static BGSDefaultObjectManager* GetSingleton() { static BGSDefaultObjectManager d; return present?&d:nullptr; }
    template<class T> T** GetObject(DefaultObjectID) { return slot ? reinterpret_cast<T**>(&slot) : nullptr; }
};
struct PlayerCharacter { FormID GetFormID() const { return 0x14; } };
}
'''
LOG_STUB='namespace spdlog { template<class... T> void info(const T&...){} template<class... T> void warn(const T&...){} }\n'
AUDIO_ENGINE=r'''
#include "PCH.h"
#include "fc/RuntimePolicy.hpp"
#include <cassert>
namespace fc {
struct Engine {
    bool menuOpen_{}, ready_{true}, audioAvailable16_{}, audioMuted16_{}, audioWarned16_{};
    AudioMuteLease16 audioLease16_;
    void SyncAudioFreeze16();
};
'''
AUDIO_MAIN=r'''
}
int main() {
    using M=RE::BGSDefaultObjectManager;
    fc::Engine e;
    e.SyncAudioFreeze16(); assert(M::category.writes==0);
    e.menuOpen_=true; e.SyncAudioFreeze16();
    assert(e.audioAvailable16_ && e.audioMuted16_ && M::category.volume==0.0f);
    e.SyncAudioFreeze16(); assert(M::category.writes==1);
    M::category.volume=0.25f; e.SyncAudioFreeze16(); assert(M::category.volume==0.0f);
    e.menuOpen_=false; e.SyncAudioFreeze16(); assert(M::category.volume==0.65f);
    M::category.volume=0.0f; e.menuOpen_=true; e.SyncAudioFreeze16();
    e.menuOpen_=false; e.SyncAudioFreeze16(); assert(M::category.volume==0.0f);
    M::present=false; e.menuOpen_=true; e.SyncAudioFreeze16();
    assert(!e.audioAvailable16_ && !e.audioMuted16_);
    M::present=true; M::slot=nullptr; e.SyncAudioFreeze16(); assert(!e.audioAvailable16_);
}
'''
LAUNCHER_PCH=r'''#pragma once
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>
#include <fmt/format.h>
#include "fc/Core.hpp"
namespace spdlog { template<class... T> void info(const T&...){} template<class... T> void warn(const T&...){} }
namespace RE {
using FormID=std::uint32_t; using VMTypeID=std::uint32_t; using BSFixedString=std::string;
template<class T> using BSTSmartPointer=std::shared_ptr<T>;
struct TESForm { FormID id{}; FormID GetFormID() const {return id;} int GetFormType() const {return 1;} };
struct TESObjectREFR:TESForm { template<class T> T* As() { return static_cast<T*>(this); } };
struct Actor:TESObjectREFR {
    const char* GetName() const {return "Actor";}
    bool IsDead(bool) const {return false;} bool IsDisabled() const {return false;}
};
struct PlayerCharacter:Actor { static PlayerCharacter* GetSingleton() {static PlayerCharacter p; return &p;} };
struct TESQuest:TESForm {};
struct TESDataHandler {
    static TESDataHandler* GetSingleton() {return nullptr;}
    template<class T> T* LookupForm(FormID,std::string_view) {return nullptr;}
};
namespace BSScript {
struct Object {}; struct IStackCallbackFunctor {};
struct IObjectHandlePolicy { std::uint64_t GetHandleForObject(VMTypeID,const void*) {return 1;} };
namespace Internal {
struct VirtualMachine {
    static VirtualMachine* GetSingleton() {return nullptr;}
    IObjectHandlePolicy* GetObjectHandlePolicy() {return nullptr;}
    bool FindBoundObject(std::uint64_t,const char*,BSTSmartPointer<Object>&) {return false;}
    bool DispatchMethodCall(BSTSmartPointer<Object>&,const BSFixedString&,void*,BSTSmartPointer<IStackCallbackFunctor>&) {return false;}
};
}
}
}
#include "vmargs18_commonlib_double.hpp"
'''
LAUNCHER_ENGINE=r'''#pragma once
#include "PCH.h"
namespace fc {
struct NamedRef { ID id{}; std::string name; bool waiting{}; float distance{}; std::string state; };
struct View {bool sexLabInstalled16{},sexLabBound16{},audioAvailable16{},audioMuted16{};std::string sexLabStatus16;std::vector<NamedRef> sexLabActors16;};
enum class Op {SexLabAdd16,SexLabRemove16,SexLabMove16,SexLabClear16,SexLabStart16,SexLabStopAll16};
struct Action {Op op{}; ID target{}; int count{},scope{};std::string text;};
struct Engine {
    std::vector<ID> sexLabActors16_; std::string sexLabStatus16_;
    bool audioAvailable16_{},audioMuted16_{},dirty_{};
    RE::TESObjectREFR* Ref(ID) const;
    void Note(std::string);
    void RefreshSexLab16(View&); void ApplySexLab16(const Action&);
    bool DispatchSexLab16(const Action&); bool DispatchSexLabStopAll16();
};
}
'''
def audio_function(text: str) -> str:
    start=text.index('void Engine::SyncAudioFreeze16() {')
    end=text.index('\nvoid Engine::SyncInput()',start)
    return text[start:end]
def main() -> int:
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--compiler',default='g++')
    p.add_argument('--fmt-include',required=True)
    p.add_argument('--baseline-zip',type=Path)
    p.add_argument('--output',type=Path)
    a=p.parse_args()
    compiler=shutil.which(a.compiler)
    if not compiler or not (Path(a.fmt_include)/'fmt/format.h').is_file():p.error('Compiler and real fmt headers are required')
    current={n:(ROOT/'src'/n).read_text(encoding='utf-8') for n in ('PCH.h','Engine.cpp','SexLab16.cpp')}
    if a.baseline_zip:
        with zipfile.ZipFile(a.baseline_zip) as z:
            baseline={n:z.read('FreedomControl/src/'+n).decode('utf-8') for n in current}
    else:
        baseline=dict(current)
        baseline['PCH.h']=baseline['PCH.h'].replace('#include "fc/Win32MacroCleanup17.hpp"\n','')
        baseline['SexLab16.cpp']=baseline['SexLab16.cpp'].replace('requested.push_back(playerID)','requested.push_back(kPlayer)').replace('a.target!=playerID','a.target!=kPlayer')
    logs=[]
    def run(label,command,expected_error=None,expected_success=True):
        proc=subprocess.run([str(x) for x in command],capture_output=True,text=True,errors='replace',timeout=45)
        body=proc.stdout+proc.stderr
        ok=(proc.returncode==0) if expected_success else (proc.returncode!=0 and expected_error in body)
        logs.append({'test':label,'command':[str(x) for x in command],'exit_code':proc.returncode,'expected_failure':not expected_success,'passed':ok,'output':body})
        print(('PASS: ' if ok else 'FAIL: ')+label,flush=True)
        if not ok: print(body)
        return ok
    with tempfile.TemporaryDirectory(prefix='fc-compile17-') as folder:
        top=Path(folder)
        for variant,sources in (('old',baseline),('fixed',current)):
            t=top/variant;t.mkdir()
            for d in ('RE/B','SKSE','nlohmann','spdlog'):(t/d).mkdir(parents=True,exist_ok=True)
            (t/'PCH.h').write_text(sources['PCH.h'])
            (t/'RE/Skyrim.h').write_text(AUDIO_RE)
            (t/'RE/B/BGSDefaultObjectManager.h').write_text('#include "RE/Skyrim.h"\n')
            (t/'RE/B/BGSSoundCategory.h').write_text('#include "RE/Skyrim.h"\n')
            (t/'SKSE/SKSE.h').write_text('// explicitly empty SKSE double\n')
            (t/'nlohmann/json.hpp').write_text('// no JSON used by audio boundary\n')
            (t/'spdlog/spdlog.h').write_text(LOG_STUB)
            (t/'Windows.h').write_text('#pragma once\n#ifdef UNICODE\n#define GetObject GetObjectW\n#else\n#define GetObject GetObjectA\n#endif\n')
            (t/'audio.cpp').write_text(AUDIO_ENGINE+audio_function(sources['Engine.cpp'])+AUDIO_MAIN)
            options=[compiler,'-std=c++23','-DFMT_HEADER_ONLY','-I'+str(t),'-I'+str(ROOT/'include'),'-I'+a.fmt_include]
            for unicode in (False,True):
                tag=variant+('-Unicode' if unicode else '-ANSI')
                binary=t/('audio-'+str(int(unicode)))
                cmd=options+(['-DUNICODE'] if unicode else [])+[t/'audio.cpp','-o',binary]
                ok=run(tag+'-actual-PCH-and-audio',cmd,'GetObjectW' if unicode else 'GetObjectA',variant=='fixed')
                if ok and variant=='fixed': run(tag+'-audio-lifecycle', [binary])
            if variant=='fixed':
                (t/'ApiContract17.cpp').write_text((ROOT/'src/ApiContract17.cpp').read_text())
                run('actual-API-contract-TU-with-declared-doubles',options+['-fsyntax-only',t/'ApiContract17.cpp'])
            (t/'PCH.h').write_text(LAUNCHER_PCH)
            shutil.copy2(ROOT/'tests/vmargs18_commonlib_double.hpp', t/'vmargs18_commonlib_double.hpp')
            (t/'Engine.hpp').write_text(LAUNCHER_ENGINE)
            for n in ('RE/F/FunctionArguments.h','RE/V/VirtualMachine.h'):
                (t/n).parent.mkdir(parents=True,exist_ok=True);(t/n).write_text('#include "PCH.h"\n')
            (t/'SexLab16.cpp').write_text(sources['SexLab16.cpp'])
            run(variant+'-WHOLE-launcher-TU',options+['-fsyntax-only',t/'SexLab16.cpp'],'kPlayer',variant=='fixed')
        policy=top/'wheel.cpp'
        policy.write_text('#include "fc/InputPolicy13.hpp"\nint main(){fc::input13::WheelStream w; auto v=w.Select(0.0f,0.0f,0.0f,0.0f,1.0f); return v.first==0.0f && v.second==1.0f ? 0 : 1;}\n')
        binary=top/'wheel'
        if run('wheel-policy-warning-clean',[compiler,'-std=c++20','-Wall','-Wextra','-Wconversion','-Werror','-I'+str(ROOT/'include'),policy,'-o',binary]):run('wheel-policy-execute',[binary])
    result={'scope':'Host compilation with explicitly simulated Windows/RE/Papyrus interfaces; no full game DLL','compiler':compiler,'build':(ROOT/'LATEST_BUILD_ID.txt').read_text().strip(),'checks':logs,'all_passed':all(x['passed'] for x in logs)}
    if a.output:a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_text(json.dumps(result,indent=2,ensure_ascii=False)+'\n')
    return 0 if result['all_passed'] else 1
if __name__=='__main__':raise SystemExit(main())
