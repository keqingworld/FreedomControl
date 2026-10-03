@echo off
setlocal DisableDelayedExpansion
set "FC_EXIT=1"
set "FC_BUILD="
set "FC_CMAKE="
echo FreedomControl 0.5.5 SexFast16 + Repair15 - package an EXISTING Release DLL
echo No compilation, vcpkg, downloads, or game installation.
echo.
if exist "%~dp0build\vs2026\Release\FreedomControl.dll" set "FC_BUILD=%~dp0build\vs2026"
if not defined FC_BUILD if exist "%~dp0build\vs2022\Release\FreedomControl.dll" set "FC_BUILD=%~dp0build\vs2022"
if not defined FC_BUILD goto missing_dll
if not exist "%FC_BUILD%\CMakeCache.txt" goto missing_cache
for /f "usebackq tokens=1,* delims==" %%A in ("%FC_BUILD%\CMakeCache.txt") do if "%%A"=="CMAKE_COMMAND:INTERNAL" set "FC_CMAKE=%%B"
if not defined FC_CMAKE goto missing_cmake
set "FC_CMAKE=%FC_CMAKE:/=\%"
if not exist "%FC_CMAKE%" goto missing_cmake
if not exist "%~dp0cmake\PackageRuntime.cmake" goto missing_script
"%FC_CMAKE%" "-DFC_ROOT=%~dp0." "-DFC_BUILD=%FC_BUILD%" -P "%~dp0cmake\PackageRuntime.cmake" > "%~dp0package.log" 2>&1
set "FC_EXIT=%ERRORLEVEL%"
type "%~dp0package.log"
if not "%FC_EXIT%"=="0" goto failed
echo.
echo PACKAGE COMPLETE. Install dist\FreedomControl-0.5.5-VMARGS18-MO2.zip with MO2.
goto done
:missing_dll
echo No existing Release DLL was found. Run BUILD.bat first.
goto done
:missing_cache
echo CMakeCache.txt is missing from the selected build folder.
goto done
:missing_cmake
echo The CMake executable recorded by the existing build was not found.
echo Run BUILD.bat to rediscover the installed build tools.
goto done
:missing_script
echo cmake\PackageRuntime.cmake is missing. Extract the complete Buildfix8 source ZIP.
goto done
:failed
echo.
echo Packaging failed. Read package.log. Existing compiled objects were not deleted.
:done
if defined FC_NO_PAUSE exit /b %FC_EXIT%
echo Press any key to close this window.
pause >nul
exit /b %FC_EXIT%
