# Building SavvyCAN

Targets: **Windows x86**, **Windows x64**, **Linux x64**. All code is under `src\`;
`SavvyCAN.pro` in the repo root is the single project file for every target.

Why two Windows builds: a vendor CAN driver DLL (J2534 passthru such as the XHorse / clone
MVCI, PCANBasic, vxlapi) can only be loaded by a process of the same bitness. The XHorse
DLL is 32-bit, so it needs `bin\x86\SavvyCAN.exe`.

## Dependencies

### Qt

Qt **5.15.2** (5.14 is the minimum; Qt 6 does not work because of QtSerialBus). Modules used
by `SavvyCAN.pro`:

| Module | Used for |
|---|---|
| core, gui, widgets | application |
| serialbus | CAN adapters (SocketCAN, PEAK, Vector, J2534 passthru, ...) |
| serialport | GVRET / LAWICEL serial adapters |
| qml | scripting window |
| help | help window |
| network | socketcand, CANserver, MQTT |
| printsupport, opengl | plotting (QCustomPlot) |

All of these are in the standard Qt 5.15.2 desktop install; no add-on modules are needed.
QCustomPlot and the QMQTT client are vendored in `src\`.

### Windows x86 / x64 (MSVC)

| Dependency | Version / location | Why |
|---|---|---|
| Visual Studio Build Tools, C++ workload | 2026, `C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools` | compiler, nmake, vcvars |
| Python 3 + `aqtinstall` | `python -m pip install aqtinstall` | installs the Qt kits |
| Qt 5.15.2 32-bit kit | `C:\Qt\5.15.2\msvc2019` | x86 build |
| Qt 5.15.2 64-bit kit | `C:\Qt\5.15.2\msvc2019_64` | x64 build |

Install the Qt kits:

```
python -m aqt install-qt windows desktop 5.15.2 win32_msvc2019    -O C:\Qt
python -m aqt install-qt windows desktop 5.15.2 win64_msvc2019_64 -O C:\Qt
```

The `msvc2019` kits are binary compatible with the 2022 and 2026 toolchains. One
compatibility fix is needed for 2026: its STL removed `stdext::make_checked_array_iterator`,
which Qt 5.15 headers still use, so `src\utils\msvc_stdext_shim.h` is force-included
(`/FI`) on MSVC by both `.pro` files. Without it a clean build fails in `qcustomplot.cpp`.

### Linux (Ubuntu / Debian)

```
sudo apt install build-essential qtbase5-dev qt5-qmake libqt5serialbus5-dev libqt5serialport5-dev \
     qtdeclarative5-dev qttools5-dev libqt5opengl5-dev
```

`qttools5-dev` provides the Qt Help module. On older releases `qt5-default` replaces
`qtbase5-dev qt5-qmake`. Packaging as an AppImage additionally needs
[linuxdeployqt](https://github.com/probonopd/linuxdeployqt).

### Optional tooling (not needed to build)

| Tool | Needs |
|---|---|
| `savvycan-mcp\server.py` (MCP server over the CLI) | Python 3, `pip install mcp` |
| `workspace\toggle_finder.py` (CAN bit finder) | Python 3, standard library only |
| CI (`.github\workflows\build.yml`) | GitHub-hosted runners, `jurplel/install-qt-action` |

### Runtime (Windows)

`bin\<arch>\` is self-contained: Qt DLLs, Qt plugins, `vc_redist.<arch>.exe`, `help\`,
`examples\`. Vendor adapter DLLs (e.g. the XHorse J2534 DLL) are installed by the vendor
and found through the registry / PATH; they are not part of the build.

## Windows build

From a plain command prompt (the script sets up MSVC and Qt itself):

```
build-win.cmd          both architectures
build-win.cmd x86      32-bit only
build-win.cmd x64      64-bit only
```

Per architecture the script:

1. runs `qmake` + `nmake` on `SavvyCAN.pro` into `build-win32\` or `build-win64\`
2. builds the patched passthrucan (J2534) plugin from `src\plugins\passthrucan\` into
   `src\plugins\passthrucan\out\<arch>\` and installs it into that Qt kit's `plugins\canbus`
3. recreates the release folder `bin\x86\` or `bin\x64\`: the exe, `windeployqt` output,
   the patched plugin, `help\` and `examples\`

The MCP server (`savvycan-mcp`) and the skills run `bin\<arch>\SavvyCAN.exe`; see
`savvycan-mcp\README.md` for which build to use with which adapter.

Output:

```
bin\x86\SavvyCAN.exe   32-bit (use with 32-bit vendor DLLs)
bin\x64\SavvyCAN.exe   64-bit
```

The same exe runs headless with `SavvyCAN.exe --cli --help`.

### Clean build

Do this after editing `SavvyCAN.pro`, moving files or changing the toolchain. An incremental
build can hide errors by reusing old object files.

```
clean.cmd              remove all generated output (build-win32\, build-win64\, bin\, plugin output)
clean.cmd x86          only the 32-bit output
clean.cmd x64          only the 64-bit output
build-win.cmd
```

`clean.cmd` only removes gitignored build output; it never touches sources, `help\`,
`examples\` or `workspace\`. It fails with a message if a folder is held open, for example
by a SavvyCAN.exe running from `bin\`.

### Verify

- `bin\x86\SavvyCAN.exe` is a 32-bit PE, `bin\x64\SavvyCAN.exe` a 64-bit PE
  (`file bin/x86/SavvyCAN.exe` in Git Bash, or `dumpbin /headers`).
- `bin\<arch>\canbus\qtpassthrucanbus.dll` exists with the same bitness.
- `git status` shows no build output; `bin\`, `build-win*\`, `src\plugins\passthrucan\out\`
  and `src\plugins\passthrucan\build-*\` are gitignored.

### Alternative: MSYS2 / MinGW (64-bit only)

Only if MSVC is unavailable. 32-bit vendor DLLs cannot be used with this build.

```
pacman -S mingw-w64-x86_64-qt5-base mingw-w64-x86_64-qt5-serialbus mingw-w64-x86_64-qt5-serialport \
          mingw-w64-x86_64-qt5-declarative mingw-w64-x86_64-qt5-tools mingw-w64-x86_64-qt5-svg \
          mingw-w64-x86_64-gcc mingw-w64-x86_64-make
qmake-qt5 SavvyCAN.pro
mingw32-make -j$(nproc)
windeployqt-qt5 release/SavvyCAN.exe
```

## Linux build

Out of tree, so the repo root stays clean:

```
mkdir -p build-linux && cd build-linux
qmake CONFIG+=release PREFIX=/usr ../SavvyCAN.pro
make -j$(nproc)
./SavvyCAN
```

Install (`make install` honours `PREFIX`, and installs `help\` next to the binary) or
package as CI does:

```
make INSTALL_ROOT=appdir install
linuxdeployqt appdir/usr/share/applications/SavvyCAN.desktop -appimage \
    -extra-plugins=iconengines,platformthemes/libqgtk3.so,canbus
```

`./install.sh` creates a desktop shortcut. SocketCAN and the passthru plugin come from the
distro's qtserialbus package; the Windows-only patch in `src\plugins\passthrucan\` is not needed.

## Continuous integration

`.github\workflows\build.yml` builds Linux x64 (AppImage), Windows x86 and Windows x64
(zip, with the patched passthrucan plugin) on every push to `master` and publishes them on
the `continuous` pre-release.

## Troubleshooting

| Symptom | Cause / fix |
|---|---|
| `'stdext': is not a class or namespace name` | shim not force-included; check the `/FI` line in `SavvyCAN.pro` and `src\plugins\passthrucan\passthrucan.pro` |
| `Qt for x64 not found at C:\Qt\5.15.2\msvc2019_64` | install the kit with aqtinstall (above) |
| `vcvars32.bat not found` | install VS 2026 Build Tools with the C++ workload, or edit `VSBUILD` in `build-win.cmd` |
| `'build-win.cmd' is not recognized` from Git Bash | run `cmd //c C:\projects\SavvyCAN\build-win.cmd` |
| adapter opens but no frames with a clone J2534 device | that is what the passthrucan patch fixes; make sure `bin\<arch>\canbus\qtpassthrucanbus.dll` is the patched one (rebuild) |
| 32-bit vendor DLL will not load | you are running `bin\x64`; use `bin\x86` |
