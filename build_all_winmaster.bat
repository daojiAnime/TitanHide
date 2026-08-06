@echo off
setlocal
call "D:\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=amd64 -host_arch=amd64
set MSB=D:\BuildTools\MSBuild\Current\Bin\amd64\MSBuild.exe
set KITLIB=C:\Program Files (x86)\Windows Kits\10\lib\10.0.26100.0
set MSVCLIB=D:\BuildTools\VC\Tools\MSVC\14.44.35207\lib
cd /d D:\src\TiDaoji

echo === TiDaoji.sys ===
"%MSB%" TiDaoji\TiDaoji.vcxproj /m /t:Rebuild /p:Configuration=Release /p:Platform=x64 /p:SignMode=Off /p:EnableInf2cat=false /p:Driver_SpectreMitigation=false /p:SpectreMitigation=false /v:minimal /nologo
echo SYS=%ERRORLEVEL%
if errorlevel 1 exit /b 1
if exist TiDaoji\x64\Release\TiDaoji.sys copy /Y TiDaoji\x64\Release\TiDaoji.sys D:\src\TiDaoji.sys

REM Force link.exe LIBPATH (MSBuild may ignore LIB for these projects)
set "LINK=/LIBPATH:\"%KITLIB%\um\x64\" /LIBPATH:\"%KITLIB%\ucrt\x64\" /LIBPATH:\"%MSVCLIB%\x64\""

set FORCEIMPORT=/p:ForceImportAfterCppTargets=D:\src\TiDaoji\UsermodeSdk.props

echo === TiDaojiGUI x64 ===
"%MSB%" TiDaojiGUI\TiDaojiGUI.vcxproj /m /t:Rebuild /p:Configuration=Release /p:Platform=x64 %FORCEIMPORT% /v:minimal /nologo
echo GUI=%ERRORLEVEL%

echo === TiDaoji_x64dbg x64 (.dp64) ===
"%MSB%" TiDaoji_x64dbg\TiDaoji_x64dbg.vcxproj /m /t:Rebuild /p:Configuration=Release /p:Platform=x64 %FORCEIMPORT% /v:minimal /nologo
echo DBG64=%ERRORLEVEL%

echo === TiDaoji_x64dbg Win32 (.dp32 for x32dbg) ===
call "D:\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x86 -host_arch=amd64 >nul
"%MSB%" TiDaoji_x64dbg\TiDaoji_x64dbg.vcxproj /m /t:Rebuild /p:Configuration=Release /p:Platform=Win32 %FORCEIMPORT% /v:minimal /nologo
echo DBG32=%ERRORLEVEL%
call "D:\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=amd64 -host_arch=amd64 >nul

echo === TiDaoji_TitanEngine x64 ===
"%MSB%" TiDaoji_TitanEngine\TiDaoji_TitanEngine.vcxproj /m /t:Rebuild /p:Configuration=Release /p:Platform=x64 %FORCEIMPORT% /v:minimal /nologo
echo TE=%ERRORLEVEL%

echo === TiDaoji_OllyDbg Win32 ===
call "D:\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x86 -host_arch=amd64 >nul
"%MSB%" TiDaoji_OllyDbg\TiDaoji_OllyDbg.vcxproj /m /t:Rebuild /p:Configuration=Release /p:Platform=Win32 %FORCEIMPORT% /v:minimal /nologo
echo OLLY=%ERRORLEVEL%

echo === artifacts ===
dir /s /b *.sys 2>nul
dir /s /b *.dp64 2>nul
dir /s /b *.dp32 2>nul
dir /s /b *TiDaojiGUI.exe 2>nul
dir /s /b *TiDaojiOlly.dll 2>nul
dir /s /b *TiDaojiTE.dll 2>nul
endlocal
