@echo off
setlocal
REM Profile A: temporarily clear DSE (g_CiOptions -> 0) for unsigned driver load.
REM Requires admin. KDU + drv64.dll must sit in tools\dse\kdu\
set "HERE=%~dp0"
set "KDU=%HERE%kdu\kdu.exe"
if not exist "%KDU%" (
  echo [!] missing %KDU%
  exit /b 1
)
pushd "%HERE%kdu"
echo [*] DSE OFF -^> 0  ^(kdu -dse 0^)
"%KDU%" -dse 0
set RC=%ERRORLEVEL%
popd
if not "%RC%"=="0" if not "%RC%"=="1" (
  echo [!] kdu returned %RC%
  exit /b %RC%
)
echo [+] DSE window open ^(unsigned load allowed^). Start driver, then run dse_on.bat immediately.
endlocal
exit /b 0
