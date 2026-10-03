"""Optional source/archive checks. Does NOT compile the Windows plugin or parse PowerShell grammar."""
from pathlib import Path
import hashlib
import json
import re

root = Path(__file__).resolve().parents[1]
checks = {}
for name in ("BUILD.bat", "REPAIR_BUILD8.bat", "VERIFY_LATEST.bat", "PACKAGE_ONLY.bat", "build.ps1", "build-settings.example.json", "tests/build_settings_tests.ps1", "package/SKSE/Plugins/FreedomControl.ini"):
    raw = (root / name).read_bytes()
    raw.decode("ascii")
    assert not raw.startswith(b"\xef\xbb\xbf")
    assert b"\n" not in raw.replace(b"\r\n", b"")
    checks[name + ": ASCII, no BOM, CRLF"] = True
bat = (root / "BUILD.bat").read_text()
ps = (root / "build.ps1").read_text()
assert 'DisableDelayedExpansion' in bat
assert '-File "%~dp0build.ps1"' in bat
assert 'exit /b %FC_EXIT%' in bat
assert not re.search(r'(?im)^\s*(Invoke-Expression|Set-ExecutionPolicy)\b', ps)
assert '& $Executable @Arguments' in ps
assert "'-version','[17.0,19.0)'" in ps
assert "Generator = 'Visual Studio 18 2026'" in ps
assert "MinimumCMake = [Version]'4.2.0'" in ps
assert "'-G',$Generator,'-A','x64','-T','host=x64'" in ps
assert "@(& $Candidate -E capabilities)" in ps
assert "'-T','v143'" not in ps
checks['VS2022/VS2026 detection / CMake capability checks / native default toolset'] = True
assert 'function Read-BuildSettings(' in ps
assert '$Settings = Read-BuildSettings $Root' in ps
assert '[IO.FileMode]::CreateNew' in ps
assert '$Property = $Overrides.PSObject.Properties[$Name]' in ps
assert 'Assert-SourceTree $Root' in ps
assert ps.index('Assert-SourceTree $Root') < ps.index('$Settings = Read-BuildSettings $Root')
assert ps.index('$Settings = Read-BuildSettings $Root') < ps.index('$CommonLib = Full-Path')
assert 'build-settings.example.json' not in ps
assert '$SettingsFile -Raw' not in ps
checks['Optional settings defaults / create-if-missing / partial fields / source preflight PRESENT (static only)'] = True
assert 'x64-windows-static-md' in ps
checks['BAT quoting / no delayed expansion / exit propagation'] = True
package_bat = (root / 'PACKAGE_ONLY.bat').read_text()
package_core = (root / 'cmake/PackageRuntime.cmake').read_text()
assert 'DisableDelayedExpansion' in package_bat
assert 'CMAKE_COMMAND:INTERNAL' in package_bat
assert '-P "%~dp0cmake\\PackageRuntime.cmake"' in package_bat
assert 'exit /b %FC_EXIT%' in package_bat
assert not re.search(r'(?im)^\s*(call|powershell|pwsh|git|vcpkg)\b', package_bat)
assert 'cmake\\PackageRuntime.cmake' in ps
assert "'-P',(Join-Path $Root 'cmake\\PackageRuntime.cmake')" in ps
assert 'file(SHA256' in package_core and 'file(ARCHIVE_EXTRACT' in package_core
assert 'COPYING.txt' in package_core and 'EXCEPTIONS.md' in package_core
assert 'CommonLib-licenses' in package_core
checks['Both build/standalone entry points use the tested CMake packager (static entry-point checks)'] = True
for script in ('build.ps1', 'tests/build_settings_tests.ps1'):
    code = '\n'.join(line for line in (root / script).read_text().splitlines()
                     if not line.lstrip().startswith('#'))
    assert not re.search(r'\b(Get-FileHash|Compress-Archive|Expand-Archive)\b', code)
checks['No hash/compression cmdlet invocation in build or optional settings tests'] = True
assert 'PACKAGE_ONLY records NOT_RERUN' in (root/'BUILD_STATUS.md').read_text()
checks['Validation status explicitly separates packaging, portable tests and Windows/game testing'] = True
checks['PowerShell native argument arrays / no permanent policy change'] = True
manifest = json.loads((root / 'vcpkg.json').read_text())
settings = json.loads((root / 'build-settings.example.json').read_text())
assert settings == {'CommonLibPath': '..\\CommonLibSSE-NG', 'VcpkgRoot': '', 'CMakePath': '', 'VisualStudioPath': '', 'Jobs': 4}
assert re.fullmatch(r'[0-9a-f]{40}', manifest['builtin-baseline'])
assert any(d['name'] == 'imgui' and set(d['features']) == {'dx11-binding','win32-binding'} for d in manifest['dependencies'])
checks['JSON syntax / baseline shape / ImGui backend features'] = True
header = (root / 'src/Engine.hpp').read_text()
impl = (root / 'src/Engine.cpp').read_text()
body = re.search(r'enum class Op\s*\{(.*?)\};',header,re.S).group(1)
ops = {s.strip() for s in body.split(',') if s.strip()}
dispatch = impl+(root/'src/Kernel11.cpp').read_text()+(root/'src/QuestCenter11.cpp').read_text()
cases = set(re.findall(r'case Op::(\w+)\s*:', dispatch)) | set(re.findall(r'a\.op\s*==\s*Op::(\w+)', dispatch))
assert ops == cases, (ops-cases,cases-ops)
checks[f'All {len(ops)} declared action kinds have switch/conditional handlers (not behavior proof)'] = True
plugin = (root / 'src/Plugin.cpp').read_text()
assert 'SKSEPlugin_Load' in plugin and 'SKSEPlugin_Version' in plugin
assert 'RuntimeVersion()!=REL::Version{1,6,1170,0}' in plugin
assert 'data.versionIndependenceEx=0;' in plugin
checks['Explicit Skyrim 1.6.1170 guard and export declarations present'] = True
assert 'SKSE::GetTaskInterface()' in impl and 'a.epoch == epoch_.load()' in impl
checks['Main-thread task queue and save-generation checks present'] = True
build_id=(root/'LATEST_BUILD_ID.txt').read_text().strip()
assert build_id == 'FC-0.5.5-VMARGS18-20261002-N' and 'FreedomControl_BuildID' in plugin
assert 'project(FreedomControl VERSION 0.5.5 ' in (root/'CMakeLists.txt').read_text()
assert 'REL::Version{0,5,5,0}' in plugin
assert json.loads((root/'vcpkg.json').read_text())['version-semver']=='0.5.5'
assert json.loads((root/'package/SKSE/Plugins/FreedomControl.creatures.json').read_text())['build']==build_id
assert 'FC_BUILD_ID' in (root/'src/BuildIdentity.cpp').read_text()
assert 'fc_apply_build_identity' in (root/'CMakeLists.txt').read_text()
assert 'FC_IDENTITY_MISMATCH' in package_core and '_expected_build_id' in package_core
assert "'fc_runtime_tests'" in ps and "'^fc_(core|runtime|legion|freedom|kernel|input|player_freedom)$'" in ps
assert 'PauseLease pauseLease_' in header and 'CanFinishClose' in impl
assert 'DesiredBaseValue(' in impl and 'cachedRunSpeed=' not in impl
assert 'MissingQuantity(' in impl and 'FollowProgress progress' in header
assert 'view.worldPaused' in (root/'src/Overlay.cpp').read_text()
test_cmake=(root/'tests/CMakeLists.txt').read_text()
test_names=set(re.findall(r'add_test\(NAME\s+(fc_\w+)',test_cmake))
selected=re.search(r"'-R','([^']+)'",ps).group(1)
assert len(test_names)==7 and all(re.fullmatch(selected,n) for n in test_names)
assert all("'"+n+"_tests'" in ps for n in test_names)
assert '+ fc_input + fc_player_freedom (engine-independent only)' in ps
checks['SexFast16 identity, all seven portable tests built AND selected, retained pause/close/stat/bulk/follow wiring PRESENT (static only)'] = True
sexfast=(root/'src/SexLab16.cpp').read_text()
overlay=(root/'src/Overlay.cpp').read_text()
runtime_policy=(root/'include/fc/RuntimePolicy.hpp').read_text()
assert 'src/SexLab16.cpp' in (root/'CMakeLists.txt').read_text()
assert 'SexLabFramework' in sexfast and 'QuickStart' in sexfast and 'DispatchMethodCall' in sexfast
assert 'afterClose=true' in overlay and 'SexLabPage16' in overlay
assert 'class AudioMuteLease16' in runtime_policy and 'SyncAudioFreeze16' in impl
assert 'kMasterSoundCategory' in impl and 'SetCategoryVolume' in impl
checks['SexFast16 launcher and full-panel audio silence lease wiring PRESENT (static only)'] = True

legion = (root/'src/Legion.cpp').read_text()
overlay = (root/'src/Overlay.cpp').read_text()
assert 'src/Legion.cpp' in (root/'CMakeLists.txt').read_text()
assert "'fc_legion_tests'" in ps
for item in ('MaintainLegion','PrepareLegionActor','DeathAttempt','RemoveReference','SetFollowPackage','IsolateActorFactions'):
    assert 'Engine::'+item in legion
assert 'BindFollower10(actor,state)' in legion
assert 'TESPackage::CreatePackage' not in legion
assert 'flags.set(RE::PACKAGE_DATA::GeneralFlag::kIgnoreCombat)' not in legion
assert 'GoalProgress goalProgress' in header and 'PickEnemy(threats' in legion
assert 'case Op::Banish:' in impl and 'case Op::SpawnActors:' in impl
assert 'legion["members"]' in impl and 'restoreRefs("removed",removed_)' in impl
assert 'void LegionPage()' in overlay and 'void SpawnActorsPage()' in overlay
assert 'IsolateActorFactions' in legion and 'VisitFactions' in legion
checks['Independent follower, enemy targeting, deferred readback and co-save wiring PRESENT (static only)'] = True
for name in ('Follower10.cpp','Spawner10.cpp','WorldFreedom10.cpp'):
    assert 'src/'+name in (root/'CMakeLists.txt').read_text()
assert 'player->PlaceObjectAtMe' in (root/'src/Spawner10.cpp').read_text()
assert 'CheckBirth10' in (root/'src/Spawner10.cpp').read_text()
assert 'ForceRefIntoAlias' in (root/'src/Follower10.cpp').read_text()
assert 'SKSE FreedomControlRuntime.esp FreedomControl-docs' in package_core
assert "'fc_freedom_tests'" in ps
checks['Freedom10 real package alias, live-player placement, birth readback and mandatory assets are wired'] = True
for name in ('Kernel11.cpp','QuestCenter11.cpp','KernelSave11.cpp','CrimeHooks11.cpp'):
    assert 'src/'+name in (root/'CMakeLists.txt').read_text()
assert 'std::atomic<bool>' in (root/'src/CrimeHooks11.cpp').read_text()
assert 'TickKernel11(' in impl and 'MaintainQuests11(' in impl
assert 'SaveKernel11(j)' in impl and 'LoadKernel11(j,serial)' in impl
for name in ('QuestPage','KernelPage11','PersonalPage11'):
    assert 'void '+name+'()' in overlay
assert 'personalSlot==live->slot' in overlay
assert 'kPersonalQuestSlots11 = 64' in (root/'include/fc/KernelPolicy.hpp').read_text()
assert 'scene->Stop()' not in (root/'src/Kernel11.cpp').read_text()+(root/'src/QuestCenter11.cpp').read_text()
checks['Kernel11 rule/quest/custom-task/save wiring; no nonexistent Scene::Stop call (static only)'] = True
assert not (root/'build-settings.json').exists()
assert not (root/'commonlib.lock.txt').exists()
assert not (root/'build').exists()
assert not (root/'dist').exists()
for forbidden in ('.dll','.exe','.pdb','.ttf','.ttc','.otf'):
    assert not any(p.suffix.lower()==forbidden for p in root.rglob('*') if p.is_file())
checks['Source-only: no DLL / EXE / PDB / font binaries'] = True
pch = (root/'src/PCH.h').read_text()
cleanup = (root/'include/fc/Win32MacroCleanup17.hpp').read_text()
assert '#    undef GetObject' in cleanup
assert pch.index('Win32MacroCleanup17.hpp') < pch.index('#include <RE/Skyrim.h>')
assert pch.rindex('Win32MacroCleanup17.hpp') > pch.index('#include <Windows.h>')
assert 'src/ApiContract17.cpp' in (root/'CMakeLists.txt').read_text()
assert 'const ID playerID = player->GetFormID();' in sexfast
assert 'requested.push_back(kPlayer)' not in sexfast and 'a.target!=kPlayer' not in sexfast
assert 'return {0.0f,gameY};' in (root/'include/fc/InputPolicy13.hpp').read_text()
checks['COMPILE17 SDK macro cleanup, translation-unit player ID, float wheel literal and real-target API assertions PRESENT (static only)'] = True
assert 'fc::MakeVMArguments18(' in sexfast and 'RE::MakeFunctionArguments(' not in sexfast
vmargs18=(root/'include/fc/VMArguments18.hpp').read_text()
assert 'MakeVMArguments18(Args... args)' in vmargs18
assert 'RE::MakeFunctionArguments<Args...>(std::move(args)...);' in vmargs18
assert 'is_return_convertible_v<Args>' in vmargs18
assert 'src/VMArgumentsContract18.cpp' in (root/'CMakeLists.txt').read_text()
checks['VMARGS18 copies argument values before constrained CommonLib instantiation; native contract compiled in DLL target (static only)'] = True
hashes = {}
for p in sorted(root.rglob('*')):
    if p.is_file() and p.name not in ('STATIC_CHECKS.json', 'RELEASE_SHA256.json') and '__pycache__' not in p.parts and not p.relative_to(root).as_posix().startswith('docs/validation'):
        hashes[p.relative_to(root).as_posix()] = hashlib.sha256(p.read_bytes()).hexdigest()
report = {
    'scope': 'Static text/packaging checks only; not Windows execution, API compilation, or game testing.',
    'checks': checks,
    'source_sha256': hashes,
}
(root / 'docs/STATIC_CHECKS.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
print(json.dumps(checks,indent=2))
print(f'PASS: {len(checks)} static groups; {len(hashes)} file hashes recorded.')
