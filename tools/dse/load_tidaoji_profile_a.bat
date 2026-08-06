@echo off
setlocal EnableDelayedExpansion
REM Full profile A: DSE off -> install/start TiDaoji -> DSE on -> optional smoke.
REM Run elevated from anywhere; paths resolved relative to this script.

set "DSE=%~dp0"
set "ROOT=%DSE%..\..\"
for %%I in ("%ROOT%") do set "ROOT=%%~fI\"
set "SYS=%ROOT%TiDaoji\x64\Release\TiDaoji.sys"
if not exist "%SYS%" set "SYS=%ROOT%x64\Release\TiDaoji.sys"
if not exist "%SYS%" set "SYS=D:\src\TiDaoji.sys"
if not exist "%SYS%" set "SYS=%ROOT%TiDaoji.sys"

echo === TiDaoji profile A load ===
echo ROOT=%ROOT%
echo SYS=%SYS%

if not exist "%SYS%" (
  echo [!] TiDaoji.sys not found. Build Release^|x64 first.
  exit /b 1
)

echo.
echo [1/4] DSE off
call "%DSE%dse_off.bat" || exit /b 1

echo.
echo [2/4] install + start service
copy /Y "%SYS%" "%SystemRoot%\system32\drivers\TiDaoji.sys" || exit /b 1
sc query TiDaoji >nul 2>&1
if !ERRORLEVEL! EQU 0 (
  sc stop TiDaoji >nul 2>&1
  timeout /t 6 /nobreak >nul 2>&1
  sc delete TiDaoji >nul 2>&1
)
sc create TiDaoji binPath= \??\%SystemRoot%\system32\drivers\TiDaoji.sys type= kernel start= demand || exit /b 1
sc start TiDaoji
if errorlevel 1 (
  echo [!] sc start failed — still run dse_on to restore DSE
  call "%DSE%dse_on.bat"
  exit /b 1
)
sc query TiDaoji

echo.
echo [3/4] DSE on ^(restore^)
call "%DSE%dse_on.bat" || exit /b 1

echo.
echo [4/4] optional smoke
if exist "%ROOT%tools\tidaoji_smoke.exe" (
  "%ROOT%tools\tidaoji_smoke.exe"
) else if exist "%ROOT%tools\tidaoji_smoke.cpp" (
  echo [*] compile smoke if needed: cl /I.. tools\tidaoji_smoke.cpp
) else (
  echo [*] smoke binary not present; use TiDaojiStatus / tidaoji_smoke later
)

echo.
echo [+] profile A done. Device \\.\TiDaoji  Log C:\TiDaoji.log
echo [+] NOT PG-safe. Unload: sc stop TiDaoji
endlocal
exit /b 0
