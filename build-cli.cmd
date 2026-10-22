@echo off
setlocal
set QTDIR=C:\Qt\5.15.2\msvc2019
call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars32.bat" || exit /b 1
set PATH=%QTDIR%\bin;%PATH%
cd /d "%~dp0"
if not exist build-win32 mkdir build-win32
cd build-win32
qmake "%~dp0SavvyCAN.pro" CONFIG+=release "QMAKE_CXXFLAGS+=/MP" || exit /b 1
nmake /nologo || exit /b 1
if not exist deploy mkdir deploy
copy /y release\SavvyCAN.exe deploy\ >nul || exit /b 1
windeployqt --release --no-translations --no-opengl-sw deploy\SavvyCAN.exe >nul || exit /b 1
echo BUILD_OK: %~dp0build-win32\deploy\SavvyCAN.exe
endlocal
