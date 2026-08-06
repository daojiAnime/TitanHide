@echo off
setlocal
REM Lab residual cleanup after TiDaoji live tests (win-master style host).
REM - Stops L1 service (InfinityHook disarmed via normal unload path)
REM - Removes probe hold/go + C:\TiDaoji_probe_* + C:\TiDaoji.log
REM - Removes tools\*.obj and accidental KDU NalDrv drop next to kdu
REM Does NOT: sc delete TiDaoji, remove TiDaoji.sys, delete samples (123.dll)
REM Requires: elevated admin for sc stop

echo === TiDaoji live residual cleanup ===

echo [1/4] sc stop TiDaoji
sc stop TiDaoji >nul 2>&1
REM drain window (driver FullTeardown ~5s)
ping -n 8 127.0.0.1 >nul 2>&1
sc query TiDaoji 2>nul | findstr /i STATE

echo [2/4] kill leftover user probes
taskkill /F /IM antidebug_probe.exe >nul 2>&1
taskkill /F /IM antidebug_probe_remote.exe >nul 2>&1
taskkill /F /IM tidaoji_smoke.exe >nul 2>&1

echo [3/4] C:\ probe artifacts + log
del /f /q C:\TiDaoji_probe_hold 2>nul
del /f /q C:\TiDaoji_probe_go.txt 2>nul
del /f /q C:\TiDaoji_antidebug_probe.txt 2>nul
del /f /q C:\TiDaoji_probe_*.txt 2>nul
del /f /q C:\TiDaoji.log 2>nul

echo [4/4] build objs + KDU NalDrv drop
set "HERE=%~dp0"
del /f /q "%HERE%*.obj" 2>nul
if exist "%HERE%dse\kdu\NalDrv.sys" del /f /q "%HERE%dse\kdu\NalDrv.sys" 2>nul
if exist "D:\tools\kdu\NalDrv.sys" del /f /q "D:\tools\kdu\NalDrv.sys" 2>nul
if exist "D:\tools\kdu\NalDrv*" del /f /q "D:\tools\kdu\NalDrv*" 2>nul

echo.
echo [+] done. Service may still be registered STOPPED; re-start needs DSE window.
echo [+] Optional dual-IH zombie: sc delete TitanHide  ^(only if unused^)
echo [+] Optional mac junk: del /s /q D:\src\TiDaoji\._*
endlocal
exit /b 0
