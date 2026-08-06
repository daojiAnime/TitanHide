@echo off
setlocal
REM TiDaoji install helper (PR5). Requires temporary DSE window (profile A).
REM Full runbook: docs\2026-08-07-tidaoji-dsu-profile-a-runbook.md
REM NOT PG-safe. Do NOT dual-load with CR / other InfinityHook.

set SYS=%~dp0x64\Release\TiDaoji.sys
if not exist "%SYS%" set SYS=%~dp0TiDaoji.sys
if not exist "%SYS%" (
  echo [!] TiDaoji.sys not found. Build TiDaoji.sln Release^|x64 first.
  exit /b 1
)

echo [*] Install TiDaoji from: %SYS%
echo [*] Ensure DSE allow-unsigned window is OPEN, then restore DSE immediately after start.
copy /Y "%SYS%" "%SystemRoot%\system32\drivers\TiDaoji.sys" || exit /b 1
sc query TiDaoji >nul 2>&1
if %ERRORLEVEL%==0 (
  sc stop TiDaoji >nul 2>&1
  timeout /t 6 /nobreak >nul
  sc delete TiDaoji >nul 2>&1
)
sc create TiDaoji binPath= %SystemRoot%\system32\drivers\TiDaoji.sys type= kernel || exit /b 1
sc start TiDaoji || exit /b 1
sc query TiDaoji
echo [*] Device path: \\.\TiDaoji  (service name == device name)
echo [*] Log: C:\TiDaoji.log or DebugView ([TIDAOJI] / [TIDAOJI][IH])
echo [*] Next: restore DSE, then x64dbg TiDaojiStatus / TiDaoji
echo [*] Runbook: docs\2026-08-07-tidaoji-dsu-profile-a-runbook.md
endlocal
