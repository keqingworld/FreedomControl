"""Reproduce Actor*& template rejection, then test the production value adapter.

The engine/VM are explicit TEST DOUBLES with constrained argument types, not a
Windows SDK or full CommonLib compile. The complete source TU is compiled, and
both the previous call expression and reference-deduction mutant must FAIL.
The real-header contract is also in the DLL's ordinary CMake target.
"""
from __future__ import annotations
import argparse
import json
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import zipfile
from compile17_regression_tests import LAUNCHER_PCH, LAUNCHER_ENGINE

ROOT = Path(__file__).resolve().parents[1]
PRELUDE = r'''
#include <array>
#include <cassert>
#include <memory>
#include <string>
namespace RE { struct Actor { int id{}; }; using BSFixedString = std::string; }
#include "vmargs18_commonlib_double.hpp"
#include "fc/VMArguments18.hpp"
'''
POSITIVE = r'''
int main() {
    RE::Actor a{1}, b{2}, c{3}, d{4}, e{5};
    std::array<RE::Actor*,5> originals{&a,&b,&c,&d,&e};
    using RE::BSScript::Variable;
    using RE::BSScript::IFunctionArguments;
    RE::BSScrapArray<Variable> out;
    // Exercise the exact eight-argument expression with 0..5 occupied slots.
    for (unsigned count=0; count<=5; ++count) {
        std::array<RE::Actor*,5> slots{};
        for (unsigned i=0;i<count;++i) slots[i]=originals[i];
        const auto snapshot=slots;
        RE::BSFixedString channel="channel", filter="filter";
        std::unique_ptr<IFunctionArguments> packed(fc::MakeVMArguments18(
            slots[0],slots[1],slots[2],slots[3],slots[4],
            static_cast<RE::Actor*>(nullptr),channel,filter));
        assert(slots==snapshot && channel=="channel" && filter=="filter");
        // A queued VM call must not refer to the caller's mutable pointer array.
        slots.fill(nullptr); channel="changed"; filter.clear();
        assert((*packed)(out) && out.size()==8);
        for(unsigned i=0;i<5;++i) assert(std::any_cast<RE::Actor*>(out[i].value)==snapshot[i]);
        assert(std::any_cast<RE::Actor*>(out[5].value)==nullptr);
        assert(std::any_cast<RE::BSFixedString>(out[6].value)=="channel");
        assert(std::any_cast<RE::BSFixedString>(out[7].value)=="filter");
    }
    // A const array yields Actor* const&, which must also be normalized.
    const std::array<RE::Actor*,5> constant=originals;
    const RE::BSFixedString label="constant";
    std::unique_ptr<IFunctionArguments> constArgs(fc::MakeVMArguments18(constant[0],label));
    assert((*constArgs)(out) && out.size()==2 && std::any_cast<RE::Actor*>(out[0].value)==&a);
    // Copies survive the caller's locals going out of scope.
    std::unique_ptr<IFunctionArguments> queued;
    {
        RE::Actor* local=&b; RE::BSFixedString text="lifetime";
        queued.reset(fc::MakeVMArguments18(local,text));
    }
    assert((*queued)(out) && std::any_cast<RE::Actor*>(out[0].value)==&b);
    assert(std::any_cast<RE::BSFixedString>(out[1].value)=="lifetime");
    std::unique_ptr<IFunctionArguments> zero(fc::MakeVMArguments18());
    assert((*zero)(out) && out.empty());
    bool enabled=true; std::int32_t number=17; float factor=0.25f;
    std::unique_ptr<IFunctionArguments> values(fc::MakeVMArguments18(enabled,number,factor));
    assert((*values)(out) && out.size()==3);
    assert(std::any_cast<bool>(out[0].value) && std::any_cast<std::int32_t>(out[1].value)==17);
    assert(std::any_cast<float>(out[2].value)==factor);
}
'''
NEGATIVE = r'''
int main(){ std::array<RE::Actor*,5> actors{};
    auto* args=RE::MakeFunctionArguments(
        actors[0],actors[1],actors[2],actors[3],actors[4],
        static_cast<RE::Actor*>(nullptr), RE::BSFixedString("channel"),RE::BSFixedString("filter"));
    (void)args;
}
'''

def main() -> int:
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler',default='g++')
    parser.add_argument('--fmt-include',required=True)
    parser.add_argument('--baseline-zip',type=Path)
    parser.add_argument('--sanitize',action='store_true')
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args()
    compiler=shutil.which(args.compiler)
    if not compiler or not (Path(args.fmt_include)/'fmt/format.h').is_file():
        parser.error('Compiler and real fmt headers required')
    results=[]
    def run(label,command,expected_ok=True,diagnostic=None):
        command=list(map(str,command))
        result=subprocess.run(command,capture_output=True,text=True,errors='replace',timeout=70)
        text=result.stdout+result.stderr
        passed=(result.returncode==0)==expected_ok
        if not expected_ok and diagnostic: passed=passed and bool(re.search(diagnostic,text,re.I))
        results.append(dict(name=label,command=command,exit_code=result.returncode,expected_success=expected_ok,passed=passed,output=text))
        print(('PASS ' if passed else 'FAIL ')+label,flush=True)
        return passed
    with tempfile.TemporaryDirectory(prefix='fc-vmargs18-') as temporary:
        t=Path(temporary)
        shutil.copy2(ROOT/'tests/vmargs18_commonlib_double.hpp',t/'vmargs18_commonlib_double.hpp')
        for name in ('RE/F/FunctionArguments.h','RE/V/VirtualMachine.h'):
            p=t/name;p.parent.mkdir(parents=True,exist_ok=True)
            p.write_text('#include "vmargs18_commonlib_double.hpp"\n')
        options=[compiler,'-std=c++23','-Wall','-Wextra','-Wpedantic','-Werror','-I'+str(t),'-I'+str(ROOT/'include')]
        if args.sanitize: options+=['-fsanitize=address,undefined','-fno-omit-frame-pointer']
        p=t/'negative.cpp';p.write_text(PRELUDE+NEGATIVE)
        run('old-eight-lvalue-call-MUST-FAIL',options+['-fsyntax-only',p],False,r'(incomplete|undefined|implicit instantiation).*')
        p=t/'values.cpp';p.write_text(PRELUDE+POSITIVE)
        binary=t/'values'
        if run('production-value-adapter-compile',options+[p,'-o',binary]):
            run('value-copy-null-count-order-const-lifetime-zero-args-execute',[binary])
        for label,expr in [('untyped-null','nullptr'),('unsupported-pointer','static_cast<void*>(nullptr)')]:
            p=t/(label+'.cpp');p.write_text(PRELUDE+'\nint main(){ (void)fc::MakeVMArguments18('+expr+'); }\n')
            run(label+'-MUST-FAIL',options+['-fsyntax-only',p],False,r'Unsupported Papyrus argument type')
        (t/'PCH.h').write_text(LAUNCHER_PCH)
        (t/'Engine.hpp').write_text(LAUNCHER_ENGINE)
        current=(ROOT/'src/SexLab16.cpp').read_text(encoding='utf-8')
        if args.baseline_zip:
            with zipfile.ZipFile(args.baseline_zip) as z:
                baseline=z.read('FreedomControl/src/SexLab16.cpp').decode('utf-8')
        else:
            baseline=current.replace('fc::MakeVMArguments18(', 'RE::MakeFunctionArguments(')
        whole_options=options+['-DFMT_HEADER_ONLY','-I'+args.fmt_include]
        for label,body,expected in [('COMPILE17-whole-TU',baseline,False),('VMARGS18-whole-TU',current,True),
            ('reference-regression-mutant',current.replace('fc::MakeVMArguments18(', 'RE::MakeFunctionArguments('),False)]:
            p=t/(label+'.cpp');p.write_text(body,encoding='utf-8')
            run(label,whole_options+['-fsyntax-only',p],expected,r'(incomplete|undefined|implicit instantiation).*')
        p=t/'VMArgumentsContract18.cpp';p.write_bytes((ROOT/'src/VMArgumentsContract18.cpp').read_bytes())
        run('real-target-contract-TU-with-explicit-doubles',whole_options+['-fsyntax-only',p])
    summary={'build':(ROOT/'LATEST_BUILD_ID.txt').read_text().strip(),'scope':__doc__,
             'compiler':compiler,'sanitizers':args.sanitize,'results':results,
             'all_passed':all(x['passed'] for x in results)}
    args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.write_text(json.dumps(summary,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    return 0 if summary['all_passed'] else 1

if __name__=='__main__': raise SystemExit(main())
