@echo off
setlocal DisableDelayedExpansion
set "ROOT=%~dp0"
set "FAIL=0"
findstr /C:"FC-0.5.5-VMARGS18-20261002-N" "%ROOT%LATEST_BUILD_ID.txt" >nul || set "FAIL=1"
findstr /C:"FreedomControl 0.5.5 SexFast16 + COMPILE17 + VMARGS18 zh-CN experimental" "%ROOT%src\Plugin.cpp" >nul || set "FAIL=1"
findstr /C:"FreedomControl-0.5.5-VMARGS18-MO2.zip" "%ROOT%cmake\PackageRuntime.cmake" >nul || set "FAIL=1"
findstr /C:"SexLabFramework" "%ROOT%src\SexLab16.cpp" >nul || set "FAIL=1"
findstr /C:"QuickStart" "%ROOT%src\SexLab16.cpp" >nul || set "FAIL=1"
findstr /C:"AudioMuteLease16" "%ROOT%include\fc\RuntimePolicy.hpp" >nul || set "FAIL=1"
findstr /C:"kMasterSoundCategory" "%ROOT%src\Engine.cpp" >nul || set "FAIL=1"
findstr /C:"SexLabPage16" "%ROOT%src\Overlay.cpp" >nul || set "FAIL=1"
findstr /C:"Win32MacroCleanup17.hpp" "%ROOT%src\PCH.h" >nul || set "FAIL=1"
findstr /C:"const ID playerID = player->GetFormID();" "%ROOT%src\SexLab16.cpp" >nul || set "FAIL=1"
findstr /C:"return {0.0f,gameY};" "%ROOT%include\fc\InputPolicy13.hpp" >nul || set "FAIL=1"
if not exist "%ROOT%include\fc\Win32MacroCleanup17.hpp" set "FAIL=1"
if not exist "%ROOT%src\ApiContract17.cpp" set "FAIL=1"
if not exist "%ROOT%include\fc\VMArguments18.hpp" set "FAIL=1"
if not exist "%ROOT%src\VMArgumentsContract18.cpp" set "FAIL=1"
findstr /C:"fc::MakeVMArguments18" "%ROOT%src\SexLab16.cpp" >nul || set "FAIL=1"
if not exist "%ROOT%src\BuildIdentity.cpp" set "FAIL=1"
if not exist "%ROOT%src\SexLab16.cpp" set "FAIL=1"
if not exist "%ROOT%cmake\SourceIdentity.cmake" set "FAIL=1"
if not exist "%ROOT%cmake\ResetPluginCache.cmake" set "FAIL=1"
if not exist "%ROOT%tests\runtime_tests.cpp" set "FAIL=1"
if not exist "%ROOT%tests\legion_tests.cpp" set "FAIL=1"
if not exist "%ROOT%src\Legion.cpp" set "FAIL=1"
if not exist "%ROOT%src\Follower10.cpp" set "FAIL=1"
if not exist "%ROOT%src\Spawner10.cpp" set "FAIL=1"
if not exist "%ROOT%src\WorldFreedom10.cpp" set "FAIL=1"
if not exist "%ROOT%src\Kernel11.cpp" set "FAIL=1"
if not exist "%ROOT%src\QuestCenter11.cpp" set "FAIL=1"
if not exist "%ROOT%src\KernelSave11.cpp" set "FAIL=1"
if not exist "%ROOT%src\CrimeHooks11.cpp" set "FAIL=1"
if not exist "%ROOT%src\PlayerFreedom14.cpp" set "FAIL=1"
if not exist "%ROOT%package\FreedomControlRuntime.esp" set "FAIL=1"
if not exist "%ROOT%package\SKSE\Plugins\FreedomControl.creatures.json" set "FAIL=1"
if "%FAIL%"=="0" (
  echo VERIFIED: FC-0.5.5-VMARGS18-20261002-N
  exit /b 0
)
echo VERIFY FAILED: this is not the expected full source tree.
exit /b 1
