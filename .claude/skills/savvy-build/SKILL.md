---
name: savvy-build
description: Build SavvyCAN on Windows 32-bit and 64-bit (release folders bin\x86 and bin\x64 via build-win.cmd) or Linux (qmake/make, AppImage). Use when asked to build, rebuild, clean-build or package SavvyCAN, or when a build fails.
---

# Build SavvyCAN

`SavvyCAN.pro` in the repo root is the single project file on every OS; all code is under
`src\`. Nothing is written outside the repo. Targets: Windows x86, Windows x64, Linux.
`BUILD.md` in the repo root is the full build and dependency reference; read it first if
anything here is unclear, and update both when the build changes.
The Windows section is the one exercised on the user's machine; Linux follows
`README.md` and `.github\workflows\build.yml` and has not been run locally.

# Windows (MSVC, primary)

Everything is driven by `build-win.cmd` in the repo root. Never add another build script;
extend that one.

## Commands

Run from a plain shell (the script sets up MSVC and Qt itself):

```
cmd /c C:\projects\SavvyCAN\build-win.cmd          # both architectures
cmd /c C:\projects\SavvyCAN\build-win.cmd x86      # 32-bit only (XHorse / 32-bit J2534 DLLs)
cmd /c C:\projects\SavvyCAN\build-win.cmd x64      # 64-bit only
```

Builds take minutes. Run in the background and log inside the repo:

```
cmd /c C:\projects\SavvyCAN\build-win.cmd > bin\build.log 2>&1
```

Then grep the log for `===`, `error`, `Done` and check the exit code.

## What a run does, per architecture

1. `qmake` + `nmake` of `SavvyCAN.pro` into `build-win32\` or `build-win64\`
2. builds the patched passthrucan (J2534) plugin from `src\plugins\passthrucan\` into
   `src\plugins\passthrucan\out\<arch>\` and installs it into the Qt kit's `plugins\canbus`
3. recreates the release folder `bin\x86\` or `bin\x64\` with the exe, `windeployqt`
   output, the patched plugin, `help\` and `examples\`. The MCP server and skills run
   `bin\<arch>\SavvyCAN.exe` (x86 for 32-bit vendor DLLs such as the XHorse J2534).

## Clean build

Run `clean.cmd` first, then build:

```
cmd /c C:\projects\SavvyCAN\clean.cmd          # all generated output; or "clean.cmd x86" / "clean.cmd x64"
cmd /c C:\projects\SavvyCAN\build-win.cmd
```

`clean.cmd` removes only gitignored output (`build-win32\`, `build-win64\`, `bin\`,
plugin `out\` and `build-*\`). It exits 1 and names the folder if something holds it open.

Do a clean build after changing `SavvyCAN.pro`, moving files, or switching toolchains.
An incremental build can hide errors by reusing old object files.

## Verify

- `bin\x86\SavvyCAN.exe` is `PE32 ... Intel i386`; `bin\x64\SavvyCAN.exe` is `PE32+ ... x86-64`
  (`file` in Git Bash shows this).
- `bin\<arch>\canbus\qtpassthrucanbus.dll` exists and has the same bitness.
- `git status` shows no build output: `bin\`, `build-win*\`, `src\plugins\passthrucan\out\` are gitignored.

## Requirements and known failures

- Visual Studio 2026 Build Tools at `C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools`.
- Qt 5.15.2 kits at `C:\Qt\5.15.2\msvc2019` and `C:\Qt\5.15.2\msvc2019_64`. Missing kit:
  `python -m aqt install-qt windows desktop 5.15.2 win32_msvc2019 -O C:\Qt` (or `win64_msvc2019_64`).
- `'stdext': is not a class or namespace name`: the VS 2026 STL removed
  `stdext::make_checked_array_iterator`; `src\utils\msvc_stdext_shim.h` must be force-included
  (`win32-msvc: QMAKE_CXXFLAGS += /FI...` in both .pro files).
- `'build-win.cmd' is not recognized` when launched from Git Bash: pass the full Windows path.

# Windows (MSYS2 / MinGW, alternative)

Only if MSVC is unavailable. 64-bit only; vendor 32-bit DLLs will not load.

```
pacman -S mingw-w64-x86_64-qt5-base mingw-w64-x86_64-qt5-serialbus mingw-w64-x86_64-qt5-serialport mingw-w64-x86_64-qt5-declarative mingw-w64-x86_64-qt5-tools mingw-w64-x86_64-qt5-svg mingw-w64-x86_64-gcc mingw-w64-x86_64-make
qmake-qt5 SavvyCAN.pro
mingw32-make -j$(nproc)
windeployqt-qt5 release/SavvyCAN.exe
```

# Linux

Qt 5.15 dev packages (base, serialbus, serialport, qml/declarative, help, opengl) and a
C++17 compiler. Build out of tree so the root stays clean:

```
mkdir -p build-linux && cd build-linux
qmake CONFIG+=release PREFIX=/usr ../SavvyCAN.pro
make -j$(nproc)
./SavvyCAN
```

Install or package (as CI does):

```
make INSTALL_ROOT=appdir install
linuxdeployqt appdir/usr/share/applications/SavvyCAN.desktop -appimage -extra-plugins=iconengines,platformthemes/libqgtk3.so,canbus
```

`help\` must sit next to the binary (or be installed under `$PREFIX/bin/help`, which
`SavvyCAN.pro` does) for the help window to work. The SocketCAN and passthru plugins come
from the distro's qtserialbus package.

# Continuous integration

`.github\workflows\build.yml` builds Linux x64 (AppImage) and Windows x86 + x64 (zip,
including the patched passthrucan plugin) on every push to master and publishes them on
the `continuous` pre-release. If a local change touches the build, check
that the workflow still matches `SavvyCAN.pro` and `build-win.cmd`.

# Common to all platforms

- After moving files or editing `SavvyCAN.pro`, do a clean build.
- Keep `bin\`, `build-*\` and `src\plugins\passthrucan\out\` out of git (they are gitignored).
