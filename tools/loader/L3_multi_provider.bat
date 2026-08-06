@echo off
setlocal EnableDelayedExpansion
REM L3: multi-environment / alternate BYOVD mapper front-end.
REM Uses TIDAOJI_MAPPER + optional provider id. No exploit code in-tree.

set "HERE=%~dp0"
set "ROOT=%HERE%..\..\"
for %%I in ("%ROOT%") do set "ROOT=%%~fI\"

if "%TIDAOJI_MAPPER%"=="" set "TIDAOJI_MAPPER=%KDMAPPER%"
if "%TIDAOJI_MAPPER%"=="" (
  echo [!] Set TIDAOJI_MAPPER or KDMAPPER to your mapper EXE
  echo     Optional: TIDAOJI_MAPPER_ARGS  TIDAOJI_PROVIDER
  echo     See providers.example.ini
  exit /b 1
)
if not exist "%TIDAOJI_MAPPER%" (
  echo [!] mapper not found: %TIDAOJI_MAPPER%
  exit /b 1
)

set "SYS=%TIDAOJI_SYS%"
if "%SYS%"=="" set "SYS=%ROOT%TiDaoji\x64\Release\TiDaoji.sys"
if not exist "%SYS%" set "SYS=%ROOT%x64\Release\TiDaoji.sys"
if not exist "%SYS%" set "SYS=D:\src\TiDaoji.sys"
if not exist "%SYS%" set "SYS=%ROOT%TiDaoji.sys"
if not exist "%SYS%" (
  echo [!] TiDaoji.sys not found
  exit /b 1
)

echo === L3 multi-provider map ===
echo MAPPER=%TIDAOJI_MAPPER%
echo ARGS=%TIDAOJI_MAPPER_ARGS%
echo PROVIDER=%TIDAOJI_PROVIDER%
echo SYS=%SYS%

REM Generic invocation patterns — adjust to your tool CLI.
if not "%TIDAOJI_PROVIDER%"=="" (
  REM Common kdmapper-style: -prv id
  "%TIDAOJI_MAPPER%" -prv %TIDAOJI_PROVIDER% %TIDAOJI_MAPPER_ARGS% "%SYS%"
) else (
  "%TIDAOJI_MAPPER%" %TIDAOJI_MAPPER_ARGS% "%SYS%"
)
set RC=!ERRORLEVEL!
echo [*] mapper exit=!RC!

if exist "%ROOT%tools\tidaoji_smoke.exe" "%ROOT%tools\tidaoji_smoke.exe"
echo [+] Teardown: soft_unload.bat  ^(or reboot if map left no device^)
endlocal & exit /b %RC%
