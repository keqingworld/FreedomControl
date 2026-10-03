"""Host-compile Legion.cpp, Follower10.cpp, Spawner10.cpp and WorldFreedom10.cpp
with explicit engine TEST DOUBLES.

This catches C++ control-flow regressions in the new implementation; it does not
compile real CommonLib, validate offsets/ABI or prove Skyrim behavior. Temporary
headers NEVER go into the plugin build. No test binary is distributed.
"""
from pathlib import Path
import argparse
import shutil
import subprocess
import tempfile
import hashlib
import zipfile
from runtime_fixture15 import write_fixture

ROOT = Path(__file__).resolve().parents[1]

def main():
    p = argparse.ArgumentParser()
    p.add_argument('--compiler', default='g++')
    p.add_argument('--sanitize', action='store_true')
    p.add_argument('--fmt-include', help='Path containing real fmt/format.h; enables compile-time format checking')
    p.add_argument('--baseline-legion', type=Path, help='Optional prior source ZIP for a compiled negative control')
    args = p.parse_args()
    # The UI dispatch must use the tested production helper, not a second,
    # independently implemented wait path that could bypass global freeze.
    engine_source = (ROOT/'src/Engine.cpp').read_text()
    wait_case = engine_source.split('case Op::Wait:',1)[1].split('case Op::Dismiss:',1)[0]
    assert 'WaitFollower(a.target,a.count!=0)' in wait_case
    assert 'EnableAI(' not in wait_case
    compiler = shutil.which(args.compiler)
    if not compiler:
        raise SystemExit('Requested host compiler is not available')
    with tempfile.TemporaryDirectory(prefix='fc-legion-seam-') as temp:
        t = Path(temp)
        write_fixture(t/'RuntimeESPFixture15.hpp')
        shutil.copy2(ROOT/'tests/seam/PCH.h', t/'PCH.h')
        
        for name in ('Legion.cpp','Follower10.cpp','Spawner10.cpp','WorldFreedom10.cpp','Runtime10.hpp','CrimeHooks11.hpp'):
            shutil.copy2(ROOT/'src'/name,t/name)
        # Expose private state to the harness only. Production header remains unchanged.
        (t/'Engine.hpp').write_text((ROOT/'src/Engine.hpp').read_text().replace('private:', 'public:'), encoding='utf-8')
        for header in ('RE/B/BGSScene.h', 'RE/T/TESLevCharacter.h','RE/T/TESGlobal.h','RE/E/ExtraMapMarker.h','RE/E/ExtraTeleport.h','RE/T/TESSpellCastEvent.h','RE/T/TESObjectLoadedEvent.h','RE/S/ScriptEventSourceHolder.h'):
            out=t/header; out.parent.mkdir(parents=True,exist_ok=True); out.write_text('// Test double provided by PCH.h\n')
        cmd=[compiler, '-std=c++23', '-Wall', '-Wextra', '-Wpedantic', '-g', '-I'+str(t), '-I'+str(ROOT/'include')]
        if args.fmt_include:
            if not (Path(args.fmt_include)/'fmt/format.h').is_file():
                raise SystemExit('Real fmt/format.h was not found at --fmt-include')
            cmd+=['-DFC_REAL_FMT11', '-I'+args.fmt_include]
        if args.sanitize:
            cmd+=['-fsanitize=address,undefined', '-fno-omit-frame-pointer']
        cmd += [str(t/'Legion.cpp'),str(t/'Follower10.cpp'),str(t/'Spawner10.cpp'),str(t/'WorldFreedom10.cpp'),str(ROOT/'tests/seam/legion_seam.cpp'),str(ROOT/'src/Core.cpp'),'-o',str(t/'seam')]
        subprocess.run(cmd,check=True,timeout=60)
        subprocess.run([str(t/'seam')],check=True,timeout=20)
        if args.baseline_legion:
            with zipfile.ZipFile(args.baseline_legion) as archive:
                old_legion=archive.read('FreedomControl/src/Legion.cpp')
            (t/'Legion.cpp').write_bytes(old_legion)
            subprocess.run(cmd,check=True,timeout=60)
            failed=subprocess.run([str(t/'seam')],cwd=t,capture_output=True,text=True,timeout=20)
            test_source=(ROOT/'tests/seam/legion_seam.cpp').read_text()
            assertion='check(deferred.packagePuts==0 && deferred.resetEvaluations15==0);'
            line=test_source[:test_source.index(assertion)].count('\n')+1
            assert failed.returncode!=0 and ('line '+str(line)) in failed.stderr, failed.stdout+failed.stderr
            print('PASS negative control: exact prior Legion.cpp rejected at asynchronous non-reset selection assertion; baseline SHA256 '+hashlib.sha256(args.baseline_legion.read_bytes()).hexdigest())

if __name__=='__main__':main()
