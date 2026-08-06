@echo off
setlocal
REM L2: manual-map TiDaoji.sys via external kdmapper-class tool.
REM Does NOT vendor exploit drivers. Set KDMAPPER to your mapper EXE.
REM Driver supports DriverObject==NULL (IoCreateDriver) + SoftUnload.

set "HERE=%~dp0"
set "ROOT=%HERE%..\..\"
for %%I in ("%ROOT%") do set "ROOT=%%~fI\"

if "%KDMAPPER%"=="" (
  echo [!] Set KDMAPPER to full path of kdmapper.exe ^(or compatible^)
  echo     example: set KDMAPPER=C:\lab\kdmapper.exe
  exit /b 1
)
if not exist "%KDMAPPER%" (
  echo [!] KDMAPPER not found: %KDMAPPER%
  exit /b 1
)

set "SYS=%TIDAOJI_SYS%"
if "%SYS%"=="" set "SYS=%ROOT%TiDaoji\x64\Release\TiDaoji.sys"
if not exist "%SYS%" set "SYS=%ROOT%x64\Release\TiDaoji.sys"
if not exist "%SYS%" set "SYS=D:\src\TiDaoji.sys"
if not exist "%SYS%" set "SYS=%ROOT%TiDaoji.sys"
if not exist "%SYS%" (
  echo [!] TiDaoji.sys not found. Build Release^|x64 or set TIDAOJI_SYS=
  exit /b 1
)

echo === L2 kdmapper-class map ===
echo MAPPER=%KDMAPPER%
echo SYS=%SYS%
echo [*] Preflight ^(manual^): HVCI off; blocklist if 0xC0000603; no \\Device\\Nal leftover
echo [*] Do NOT use mapper --free for long-lived TiDaoji
echo [*] Mapping... ^(DriverEntry must return; SoftUnload later; reboot to fully clean pages^)
echo [*] Docs: docs\2026-08-07-tidaoji-loader-profiles-L1-L3.md
"%KDMAPPER%" "%SYS%"
set RC=%ERRORLEVEL%
echo [*] mapper exit=%RC%
if "%RC%"=="0" goto probe
echo [!] non-zero exit — common: 0xC0000603 blocklist, 0xC0000022 AV/AC, Nal in use, pattern/offset
goto end

:probe
echo [*] Probe device \\.\TiDaoji
if exist "%ROOT%tools\tidaoji_smoke.exe" (
  "%ROOT%tools\tidaoji_smoke.exe"
) else (
  echo [*] Build tools\tidaoji_smoke.exe for HidePid smoke
)

echo [+] If device open works, use soft_unload.bat when done.
echo [+] sc stop will NOT work for pure manual-map ^(no SCM service^).

:end
endlocal
exit /b %RC%
