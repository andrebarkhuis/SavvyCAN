@echo off
REM Usage: build-win.cmd [x86 ^| x64]      (no argument: builds both)
REM
REM Builds SavvyCAN (GUI + --cli, same exe) and the patched passthrucan (J2534)
REM plugin with Qt 5.15.2 / MSVC, deploys the Qt runtime next to it and copies
REM the self-contained result to bin\<arch>:
REM
REM   bin\x86\SavvyCAN.exe   32-bit: loads 32-bit vendor DLLs (XHorse J2534, PCANBasic, ...)
REM   bin\x64\SavvyCAN.exe   64-bit: loads 64-bit vendor DLLs
REM
REM Intermediate files: build-win32\ and build-win64\ (gitignored). bin\<arch> is the only
REM deployed copy; the MCP server (savvycan-mcp) and skills run bin\<arch>\SavvyCAN.exe.
REM Qt kits are expected at C:\Qt\5.15.2\msvc2019 and C:\Qt\5.15.2\msvc2019_64:
REM   python -m aqt install-qt windows desktop 5.15.2 win32_msvc2019    -O C:\Qt
REM   python -m aqt install-qt windows desktop 5.15.2 win64_msvc2019_64 -O C:\Qt

setlocal
if not "%~1"=="" goto :single

REM No argument: build both. Plain "if errorlevel" checks on their own lines, because
REM "call ... || exit /b 1" inside a block loses the exit code when run via cmd /c.
call "%~f0" x86
if errorlevel 1 exit /b 1
call "%~f0" x64
if errorlevel 1 exit /b 1
echo.
echo Release folders:
echo   %~dp0bin\x86\SavvyCAN.exe   32-bit
echo   %~dp0bin\x64\SavvyCAN.exe   64-bit
exit /b 0

:single
set ARCH=%~1
set SRC=%~dp0
set VSBUILD=C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build

if /i "%ARCH%"=="x86" set QTDIR=C:\Qt\5.15.2\msvc2019
if /i "%ARCH%"=="x86" set VCVARS_BAT=vcvars32.bat
if /i "%ARCH%"=="x86" set BUILD=%SRC%build-win32
if /i "%ARCH%"=="x64" set QTDIR=C:\Qt\5.15.2\msvc2019_64
if /i "%ARCH%"=="x64" set VCVARS_BAT=vcvars64.bat
if /i "%ARCH%"=="x64" set BUILD=%SRC%build-win64
if not defined VCVARS_BAT ( echo Unknown architecture "%ARCH%" - use x86 or x64 & exit /b 1 )

set VCVARS=%VSBUILD%\%VCVARS_BAT%
set OUT=%SRC%bin\%ARCH%

if not exist "%VCVARS%" echo %VCVARS_BAT% not found under "%VSBUILD%"
if not exist "%VCVARS%" exit /b 1
if not exist "%QTDIR%\bin\qmake.exe" echo Qt for %ARCH% not found at %QTDIR%
if not exist "%QTDIR%\bin\qmake.exe" exit /b 1

echo === SavvyCAN %ARCH%: Qt %QTDIR%
call "%VCVARS%" >nul || exit /b 1
set PATH=%QTDIR%\bin;%PATH%

if not exist "%BUILD%" mkdir "%BUILD%"
pushd "%BUILD%" || exit /b 1
qmake "%SRC%SavvyCAN.pro" CONFIG+=release "QMAKE_CXXFLAGS+=/MP" || exit /b 1
nmake /nologo || exit /b 1
popd

REM Patched J2534 plugin; installs itself into %QTDIR%\plugins\canbus so windeployqt picks it up
call "%SRC%src\plugins\passthrucan\build-plugin.cmd" %ARCH% || exit /b 1

REM Release folder: exe + Qt runtime + patched plugin + help + examples
if exist "%OUT%" rmdir /s /q "%OUT%"
if exist "%OUT%" echo Cannot replace %OUT% - close any SavvyCAN.exe running from it and retry
if exist "%OUT%" exit /b 1
mkdir "%OUT%" || exit /b 1
copy /y "%BUILD%\release\SavvyCAN.exe" "%OUT%\" >nul || exit /b 1
windeployqt --release --no-translations --no-opengl-sw "%OUT%\SavvyCAN.exe" || exit /b 1
copy /y "%SRC%src\plugins\passthrucan\out\%ARCH%\qtpassthrucanbus.dll" "%OUT%\canbus\" >nul || exit /b 1
xcopy /e /i /y /q "%SRC%help" "%OUT%\help" >nul
xcopy /e /i /y /q "%SRC%examples" "%OUT%\examples" >nul

echo.
echo Done (%ARCH%): %OUT%\SavvyCAN.exe
endlocal
