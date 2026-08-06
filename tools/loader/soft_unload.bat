@echo off
setlocal
REM SoftUnload for L2/L3 (and optional L1). Device must still exist.
set "HERE=%~dp0"
set "ROOT=%HERE%..\..\"
for %%I in ("%ROOT%") do set "ROOT=%%~fI\"
set "NAME=%~1"
if "%NAME%"=="" set "NAME=TiDaoji"

if exist "%ROOT%tools\tidaoji_smoke.exe" (
  "%ROOT%tools\tidaoji_smoke.exe" %NAME% 0 --soft-unload
  exit /b %ERRORLEVEL%
)

echo [*] tidaoji_smoke.exe missing — compile tools\tidaoji_smoke.cpp or use plugin TiDaojiSoftUnload
echo [*] fallback: write SoftUnload via PowerShell not implemented; use GUI/plugin
exit /b 1
