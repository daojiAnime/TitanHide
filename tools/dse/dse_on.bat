@echo off
setlocal
REM Restore DSE flags. Default 6 = stock on Win10 19045 win-master.
set "HERE=%~dp0"
set "KDU=%HERE%kdu\kdu.exe"
if "%DSE_ON_VALUE%"=="" set DSE_ON_VALUE=6
if not exist "%KDU%" (
  echo [!] missing %KDU%
  exit /b 1
)
pushd "%HERE%kdu"
echo [*] DSE ON -^> %DSE_ON_VALUE%  ^(kdu -dse %DSE_ON_VALUE%^)
"%KDU%" -dse %DSE_ON_VALUE%
set RC=%ERRORLEVEL%
popd
if not "%RC%"=="0" if not "%RC%"=="1" (
  echo [!] kdu returned %RC%
  exit /b %RC%
)
echo [+] DSE restored. TiDaoji.sys if already started stays loaded ^(layer B intact^).
endlocal
exit /b 0
