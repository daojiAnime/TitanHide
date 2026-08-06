@echo off
setlocal
REM Deploy dual-arch CE plugins + keep Lua/cli for fallback.
set "HERE=%~dp0"
set "OUT=%HERE%out"
set "CE=D:\tools\CE76"
if not exist "%CE%\CE 7.6.exe" if exist "C:\Program Files\Cheat Engine 7.5" set "CE=C:\Program Files\Cheat Engine 7.5"

if not exist "%OUT%\TiDaojiCE64.dll" (
  echo [!] build first: build_ce_plugin.bat
  exit /b 1
)

if not exist "%CE%\plugins" mkdir "%CE%\plugins"
if not exist "%CE%\autorun" mkdir "%CE%\autorun"

copy /Y "%OUT%\TiDaojiCE64.dll" "%CE%\plugins\TiDaojiCE64.dll"
if exist "%OUT%\TiDaojiCE32.dll" copy /Y "%OUT%\TiDaojiCE32.dll" "%CE%\plugins\TiDaojiCE32.dll"

REM Lua fallback + CLI (panel UI)
copy /Y "%HERE%..\TiDaoji.lua" "%CE%\autorun\TiDaoji.lua" 2>nul
if exist "%HERE%..\..\tidaoji_cli.exe" copy /Y "%HERE%..\..\tidaoji_cli.exe" "%CE%\autorun\tidaoji_cli.exe"

echo.
echo [+] Deployed to %CE%
echo     plugins\TiDaojiCE64.dll  (and 32 if built)
echo     autorun\TiDaoji.lua + tidaoji_cli.exe
echo.
echo Next: restart CE -^> Edit -^> Plugins -^> add/enable TiDaojiCE64.dll
echo       Menu: TiDaoji: Hide / Unhide / Status / SoftUnload / Toggle AutoHide
endlocal
exit /b 0
