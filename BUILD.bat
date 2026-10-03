@echo off
setlocal DisableDelayedExpansion
set "FC_PS=%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe"
if exist "%SystemRoot%\Sysnative\WindowsPowerShell\v1.0\powershell.exe" set "FC_PS=%SystemRoot%\Sysnative\WindowsPowerShell\v1.0\powershell.exe"
"%FC_PS%" -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0build.ps1"
set "FC_EXIT=%ERRORLEVEL%"
echo.
if not "%FC_EXIT%"=="0" echo Build failed. See build.log in the project folder.
if "%FC_EXIT%"=="0" echo Build finished. See dist\FreedomControl-0.5.5-VMARGS18-MO2.zip.
if defined FC_NO_PAUSE exit /b %FC_EXIT%
echo Press any key to close this window.
pause >nul
exit /b %FC_EXIT%
