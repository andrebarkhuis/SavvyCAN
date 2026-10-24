@echo off
REM Usage: clean.cmd            removes all generated build output
REM        clean.cmd x86        removes only the 32-bit build output
REM        clean.cmd x64        removes only the 64-bit build output
REM
REM A clean build is:  clean.cmd  then  build-win.cmd
REM
REM Removes (all gitignored):
REM   build-win32\ build-win64\        intermediate files
REM   bin\x86\ bin\x64\ bin\build.log  release folders
REM   src\plugins\passthrucan\out\ and build-x86\ build-x64\   plugin output
REM   build-linux\                     Linux out-of-tree build, if present
REM   in-source qmake leftovers in the root (Makefile*, release\, debug\, .qmake.stash)
REM Never touches sources, help\, examples\, workspace\ or anything tracked by git.

setlocal
set SRC=%~dp0
set PLUGIN=%SRC%src\plugins\passthrucan
set FAILED=

if /i "%~1"=="x86" goto :x86
if /i "%~1"=="x64" goto :x64
if not "%~1"=="" (
    echo Unknown argument "%~1" - use x86, x64 or nothing
    exit /b 1
)

call :x86
call :x64
call :rmdir "%PLUGIN%\out"
call :rmdir "%SRC%build-linux"
call :rmdir "%SRC%release"
call :rmdir "%SRC%debug"
call :rmdir "%SRC%bin"
call :rmfiles "%SRC%Makefile" "%SRC%Makefile.Debug" "%SRC%Makefile.Release" "%SRC%.qmake.stash"
goto :done

:x86
call :rmdir "%SRC%build-win32"
call :rmdir "%SRC%bin\x86"
call :rmdir "%PLUGIN%\out\x86"
call :rmdir "%PLUGIN%\build-x86"
if "%~1"=="x86" goto :done
goto :eof

:x64
call :rmdir "%SRC%build-win64"
call :rmdir "%SRC%bin\x64"
call :rmdir "%PLUGIN%\out\x64"
call :rmdir "%PLUGIN%\build-x64"
if "%~1"=="x64" goto :done
goto :eof

:rmdir
if not exist "%~1" goto :eof
rmdir /s /q "%~1" 2>nul
if exist "%~1" (
    echo FAILED to remove %~1 - close any program using it ^(e.g. SavvyCAN.exe^) and retry
    set FAILED=1
) else (
    echo removed %~1
)
goto :eof

:rmfiles
:rmfiles_loop
if "%~1"=="" goto :eof
if exist "%~1" del /q "%~1" && echo removed %~1
shift
goto :rmfiles_loop

:done
if defined FAILED exit /b 1
echo Clean.
exit /b 0
