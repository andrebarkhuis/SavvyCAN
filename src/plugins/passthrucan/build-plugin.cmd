@echo off
REM Usage: build-plugin.cmd x86^|x64      (default: x86)
REM Builds the patched passthrucan (J2534) plugin for one architecture and installs
REM it into that Qt kit's plugins\canbus (the stock DLL is kept as .dll.orig) so
REM that windeployqt deploys the patched one. Output: out\<arch>\qtpassthrucanbus.dll
REM Normally run by the repo-root build-win.cmd.
setlocal
set ARCH=%1
if "%ARCH%"=="" set ARCH=x86
set HERE=%~dp0
set VSBUILD=C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build

if /i "%ARCH%"=="x86" set QTDIR=C:\Qt\5.15.2\msvc2019
if /i "%ARCH%"=="x86" set VCVARS_BAT=vcvars32.bat
if /i "%ARCH%"=="x64" set QTDIR=C:\Qt\5.15.2\msvc2019_64
if /i "%ARCH%"=="x64" set VCVARS_BAT=vcvars64.bat
if not defined VCVARS_BAT ( echo Unknown architecture "%ARCH%" - use x86 or x64 & exit /b 1 )
set VCVARS=%VSBUILD%\%VCVARS_BAT%

call "%VCVARS%" >nul || exit /b 1
set PATH=%QTDIR%\bin;%PATH%

set BUILD=%HERE%build-%ARCH%
if not exist "%BUILD%" mkdir "%BUILD%"
pushd "%BUILD%" || exit /b 1
qmake "%HERE%passthrucan.pro" CONFIG+=release ARCH=%ARCH% || exit /b 1
nmake /nologo || exit /b 1
popd

set PLUGDIR=%QTDIR%\plugins\canbus
if not exist "%PLUGDIR%\qtpassthrucanbus.dll.orig" copy /y "%PLUGDIR%\qtpassthrucanbus.dll" "%PLUGDIR%\qtpassthrucanbus.dll.orig" >nul
copy /y "%HERE%out\%ARCH%\qtpassthrucanbus.dll" "%PLUGDIR%\" >nul || exit /b 1
echo Patched passthrucan plugin (%ARCH%) installed into %PLUGDIR%
endlocal
