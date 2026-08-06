@echo off
setlocal
set MSBUILD=D:\BuildTools\MSBuild\Current\Bin\amd64\MSBuild.exe
cd /d D:\src\TiDaoji
echo === Building TiDaoji.sys ===
"%MSBUILD%" TiDaoji\TiDaoji.vcxproj /m /t:Rebuild /p:Configuration=Release /p:Platform=x64 /p:SignMode=Off /p:EnableInf2cat=false /p:Driver_SpectreMitigation=false /p:SpectreMitigation=false /v:minimal /nologo
echo EXIT=%ERRORLEVEL%
dir /s /b D:\src\TiDaoji\*.sys 2>nul
if exist D:\src\TiDaoji\TiDaoji\x64\Release\TiDaoji.sys copy /Y D:\src\TiDaoji\TiDaoji\x64\Release\TiDaoji.sys D:\src\TiDaoji.sys
exit /b %ERRORLEVEL%
