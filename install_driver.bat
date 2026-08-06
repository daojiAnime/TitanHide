@echo off
setlocal
set SYS=%~dp0x64\Release\TiDaoji.sys
if not exist "%SYS%" set SYS=%~dp0TiDaoji.sys
if not exist "%SYS%" (
  echo [!] TiDaoji.sys not found. Build TiDaoji.sln Release^|x64 first.
  exit /b 1
)

echo [*] Install TiDaoji from: %SYS%
copy /Y "%SYS%" "%SystemRoot%\system32\drivers\TiDaoji.sys" || exit /b 1
sc query TiDaoji >nul 2>&1
if %ERRORLEVEL%==0 (
  sc stop TiDaoji >nul 2>&1
  sc delete TiDaoji >nul 2>&1
)
sc create TiDaoji binPath= %SystemRoot%\system32\drivers\TiDaoji.sys type= kernel || exit /b 1
sc start TiDaoji || exit /b 1
sc query TiDaoji
echo [*] Device path: \\.\TiDaoji  (service name == device name)
echo [*] Log: C:\TiDaoji.log
endlocal
