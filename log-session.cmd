@echo off
REM Usage: log-session.cmd <name> [chunk-seconds] [device] [speed]
REM        log-session.cmd merge <name>
REM
REM Logs a whole drive (e.g. a cold start) with the 32-bit CLI through the XHorse / clone
REM MVCI J2534 adapter, surviving the adapter DLL's crash after roughly a minute: the
REM capture runs in chunks of <chunk-seconds> (default 45), each written to its own CSV
REM as frames arrive, and restarts immediately if a chunk ends or the process dies.
REM Start it BEFORE turning the ignition on; stop it with Ctrl+C when the drive is over.
REM
REM Output: workspace\logs\<name>\chunk-NNN.csv   (SavvyCAN native CSV, host time in
REM         microseconds since the epoch, so chunks line up across restarts)
REM         workspace\logs\<name>\session.log     one line per chunk with start time and result
REM "merge" joins all chunks of a session into workspace\logs\<name>\<name>.csv.
REM
REM Defaults: device "XHorse - MVCI", 500000 bps, 45 s chunks. Any J2534 device name that
REM canbusutil -d passthrucan lists works. Serial adapters are not the point of this script;
REM they do not crash, so run the CLI directly for those.

setlocal
set SRC=%~dp0
set EXE=%SRC%bin\x86\SavvyCAN.exe

if /i "%~1"=="merge" goto :merge
if "%~1"=="" (
    echo Usage: log-session.cmd ^<name^> [chunk-seconds] [device] [speed]
    echo        log-session.cmd merge ^<name^>
    exit /b 1
)

set NAME=%~1
set CHUNK=%~2
if "%CHUNK%"=="" set CHUNK=45
set DEVICE=%~3
if "%DEVICE%"=="" set DEVICE=XHorse - MVCI
set SPEED=%~4
if "%SPEED%"=="" set SPEED=500000
set DIR=%SRC%workspace\logs\%NAME%

if not exist "%EXE%" (
    echo %EXE% not found - run build-win.cmd x86 first
    exit /b 1
)
if not exist "%DIR%" mkdir "%DIR%"

echo Session %NAME%: device "%DEVICE%", %SPEED% bps, %CHUNK% s chunks -^> %DIR%
echo Press Ctrl+C to stop.
echo.
set /a N=0

:loop
set /a N+=1
set PAD=00%N%
set FILE=%DIR%\chunk-%PAD:~-3%.csv
echo [%DATE% %TIME%] chunk %PAD:~-3% start
echo [%DATE% %TIME%] chunk %PAD:~-3% start>> "%DIR%\session.log"
"%EXE%" --cli --type serialbus --driver passthrucan --port "%DEVICE%" --speed %SPEED% --listen-only --quiet --duration %CHUNK% --output "%FILE%" --format csv
set RC=%ERRORLEVEL%
if "%RC%"=="0" (
    echo [%DATE% %TIME%] chunk %PAD:~-3% done>> "%DIR%\session.log"
) else (
    echo [%DATE% %TIME%] chunk %PAD:~-3% ended with code %RC% ^(adapter DLL crash is normal^), restarting
    echo [%DATE% %TIME%] chunk %PAD:~-3% ended with code %RC%>> "%DIR%\session.log"
)
goto :loop

:merge
if "%~2"=="" (
    echo Usage: log-session.cmd merge ^<name^>
    exit /b 1
)
set NAME=%~2
set DIR=%SRC%workspace\logs\%NAME%
set OUT=%DIR%\%NAME%.csv
if not exist "%DIR%\chunk-001.csv" (
    echo No chunks found in %DIR%
    exit /b 1
)
REM Full paths: when launched from Git Bash, PATH may put GNU find/more first.
echo Time Stamp,ID,Extended,Dir,Bus,LEN,D1,D2,D3,D4,D5,D6,D7,D8> "%OUT%"
for %%F in ("%DIR%\chunk-*.csv") do %SystemRoot%\System32\more.com +1 "%%F" >> "%OUT%"
for /f %%C in ('%SystemRoot%\System32\find.exe /c /v "" ^< "%OUT%"') do set LINES=%%C
set /a FRAMES=%LINES%-1
echo Merged into %OUT% (%FRAMES% frames)
exit /b 0
