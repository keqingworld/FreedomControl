@echo off
setlocal
cd /d "%~dp0"
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0PUBLISH_TO_GITHUB.ps1"
echo.
if errorlevel 1 (
  echo FAILED. See the error above.
) else (
  echo DONE.
)
pause
