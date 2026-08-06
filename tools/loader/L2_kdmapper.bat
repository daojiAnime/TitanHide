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
echo [*] Mapping... ^(Entry must return quickly; SoftUnload later^)
"%KDMAPPER%" "%SYS%"
set RC=%ERRORLEVEL%
echo [*] mapper exit=%RC%

echo [*] Probe device \\.\TiDaoji
if exist "%ROOT%tools\tidaoji_smoke.exe" (
  "%ROOT%tools\tidaoji_smoke.exe"
) else (
  echo [*] Build tools\tidaoji_smoke.exe for HidePid smoke
)

echo [+] If device open works, use soft_unload.bat when done.
echo [+] sc stop will NOT work for pure manual-map ^(no SCM service^).
endlocal
exit /b %RC%
