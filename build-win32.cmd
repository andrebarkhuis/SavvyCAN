@echo off
REM Builds a 32-bit (x86) SavvyCAN with Qt 5.15.2 / MSVC so that 32-bit
REM vendor CAN driver DLLs (PCANBasic.dll, vxlapi.dll, ...) can be loaded.
REM Output: build-win32\deploy\SavvyCAN.exe (self-contained with Qt DLLs)

setlocal
set QTDIR=C:\Qt\5.15.2\msvc2019
set VCVARS="C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars32.bat"
set SRC=%~dp0
set BUILD=%SRC%build-win32

if not exist %VCVARS% ( echo vcvars32.bat not found & exit /b 1 )
if not exist "%QTDIR%\bin\qmake.exe" ( echo 32-bit Qt not found at %QTDIR% & exit /b 1 )

call %VCVARS% || exit /b 1
set PATH=%QTDIR%\bin;%PATH%

if not exist "%BUILD%" mkdir "%BUILD%"
cd /d "%BUILD%" || exit /b 1

qmake "%SRC%SavvyCAN.pro" CONFIG+=release "QMAKE_CXXFLAGS+=/MP" || exit /b 1
nmake /nologo || exit /b 1

call "%SRC%win32-passthrucanuild-plugin.cmd" || exit /b 1

if not exist deploy mkdir deploy
copy /y release\SavvyCAN.exe deploy\ >nul || exit /b 1
windeployqt --release --no-translations --no-opengl-sw deploy\SavvyCAN.exe || exit /b 1
xcopy /e /i /y /q "%SRC%help" deploy\help >nul

echo.
echo Done: %BUILD%\deploy\SavvyCAN.exe
endlocal
