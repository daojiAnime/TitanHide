@echo off
setlocal
REM Prefer L2 if KDMAPPER is configured; else L1 DSE+sc.
set "HERE=%~dp0"

if not "%KDMAPPER%"=="" if exist "%KDMAPPER%" (
  echo [*] auto: L2 ^(KDMAPPER set^)
  call "%HERE%L2_kdmapper.bat"
  exit /b %ERRORLEVEL%
)

if not "%TIDAOJI_MAPPER%"=="" if exist "%TIDAOJI_MAPPER%" (
  echo [*] auto: L3 ^(TIDAOJI_MAPPER set^)
  call "%HERE%L3_multi_provider.bat"
  exit /b %ERRORLEVEL%
)

echo [*] auto: L1 DSE + sc ^(tools\dse^)
call "%HERE%..\dse\load_tidaoji_profile_a.bat"
exit /b %ERRORLEVEL%
