@echo off
REM Builds the patched 32-bit passthrucan plugin and installs it into the Qt
REM plugin directory (so windeployqt picks it up) and into build-win32\deploy.
setlocal
set QTDIR=C:\Qt\5.15.2\msvc2019
set VCVARS="C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars32.bat"
call %VCVARS% >nul || exit /b 1
set PATH=%QTDIR%\bin;%PATH%
cd /d "%~dp0"
if not exist build mkdir build
cd build
qmake ..\passthrucan.pro CONFIG+=release || exit /b 1
nmake /nologo || exit /b 1
cd ..
if not exist "%QTDIR%\plugins\canbus\qtpassthrucanbus.dll.orig" copy /y "%QTDIR%\plugins\canbus\qtpassthrucanbus.dll" "%QTDIR%\plugins\canbus\qtpassthrucanbus.dll.orig" >nul
copy /y out\qtpassthrucanbus.dll "%QTDIR%\plugins\canbus\qtpassthrucanbus.dll" >nul || exit /b 1
if exist "%~dp0..\build-win32\deploy\canbus" copy /y out\qtpassthrucanbus.dll "%~dp0..\build-win32\deploy\canbus\" >nul
echo Patched passthrucan plugin installed.
endlocal
