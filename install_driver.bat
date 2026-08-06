@echo off
setlocal
REM TiDaoji install helper. Requires temporary DSE window (profile A).
REM Prefer one-shot: tools\dse\load_tidaoji_profile_a.bat
REM Or: tools\dse\dse_off.bat  ->  this script  ->  tools\dse\dse_on.bat
REM Full runbook: docs\2026-08-07-tidaoji-dsu-profile-a-runbook.md
REM NOT PG-safe. Do NOT dual-load with CR / other InfinityHook.

set "ROOT=%~dp0"
set SYS=%ROOT%x64\Release\TiDaoji.sys
if not exist "%SYS%" set SYS=%ROOT%TiDaoji\x64\Release\TiDaoji.sys
if not exist "%SYS%" set SYS=%ROOT%TiDaoji.sys
if not exist "%SYS%" (
  echo [!] TiDaoji.sys not found. Build TiDaoji.sln Release^|x64 first.
  exit /b 1
)

echo [*] Install TiDaoji from: %SYS%
echo [*] DSE tools tracked in tools\dse\  (dse_off.bat / dse_on.bat / kdu\)
echo [*] Ensure DSE allow-unsigned window is OPEN, then restore with dse_on.bat after start.
copy /Y "%SYS%" "%SystemRoot%\system32\drivers\TiDaoji.sys" || exit /b 1
sc query TiDaoji >nul 2>&1
if %ERRORLEVEL%==0 (
  sc stop TiDaoji >nul 2>&1
  timeout /t 6 /nobreak >nul
  sc delete TiDaoji >nul 2>&1
)
sc create TiDaoji binPath= \??\%SystemRoot%\system32\drivers\TiDaoji.sys type= kernel || exit /b 1
sc start TiDaoji || exit /b 1
sc query TiDaoji
echo [*] Device path: \\.\TiDaoji  (service name == device name)
echo [*] Log: C:\TiDaoji.log or DebugView ([TIDAOJI] / [TIDAOJI][IH])
echo [*] Next: tools\dse\dse_on.bat   then x64dbg TiDaojiStatus / TiDaoji
echo [*] One-shot: tools\dse\load_tidaoji_profile_a.bat
echo [*] Runbook: docs\2026-08-07-tidaoji-dsu-profile-a-runbook.md
endlocal
