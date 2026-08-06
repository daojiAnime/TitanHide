@echo off
setlocal EnableDelayedExpansion
REM Build TiDaoji CE native plugin for x64 and Win32.
REM Output: TiDaojiCE64.dll  TiDaojiCE32.dll  (CE plugins folder)

set "HERE=%~dp0"
set "VSDEV=D:\BuildTools\Common7\Tools\VsDevCmd.bat"
if not exist "%VSDEV%" set "VSDEV=C:\BuildTools\Common7\Tools\VsDevCmd.bat"
if not exist "%VSDEV%" (
  echo [!] VsDevCmd.bat not found
  exit /b 1
)

set "OUT=%HERE%out"
if not exist "%OUT%" mkdir "%OUT%"

echo === TiDaojiCE x64 ===
call "%VSDEV%" -arch=amd64 -host_arch=amd64 >nul
pushd "%HERE%"
cl /nologo /O2 /EHsc /LD /I"%HERE%sdk" /I"%HERE%..\..\.." ^
  /DWIN32 /D_WINDOWS /D_USRDLL /DNDEBUG ^
  TiDaojiCE.cpp /Fe:"%OUT%\TiDaojiCE64.dll" /link /DEF:TiDaojiCE.def kernel32.lib user32.lib advapi32.lib
if errorlevel 1 ( popd & exit /b 1 )
echo [+] %OUT%\TiDaojiCE64.dll
popd

echo === TiDaojiCE Win32 ===
call "%VSDEV%" -arch=x86 -host_arch=amd64 >nul
pushd "%HERE%"
cl /nologo /O2 /EHsc /LD /I"%HERE%sdk" /I"%HERE%..\..\.." ^
  /DWIN32 /D_WINDOWS /D_USRDLL /DNDEBUG ^
  TiDaojiCE.cpp /Fe:"%OUT%\TiDaojiCE32.dll" /link /DEF:TiDaojiCE.def kernel32.lib user32.lib advapi32.lib
if errorlevel 1 ( popd & exit /b 1 )
echo [+] %OUT%\TiDaojiCE32.dll
popd

dir "%OUT%\TiDaojiCE*.dll"
echo BUILD_CE_PLUGIN_OK
endlocal
exit /b 0
