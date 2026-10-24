# SavvyCAN (custom fork)

> **Self-contained project. No scratchpad, no temp directories, no external scripts.**
> Every file you create, every helper, every log and every build artifact lives inside
> this repo (`bin\`, `build-win32\`, `build-win64\` for generated output). Edit files
> directly with the editing tools; never write a helper script somewhere else to do it.
> The only scripts are `build-win.cmd` and `clean.cmd` in the repo root plus
> `src\plugins\passthrucan\build-plugin.cmd`. Extend those; do not add new ones.

Fork of SavvyCAN with a CLI mode, an MCP server, a CAN reverse-engineering workspace and
Windows 32-bit + 64-bit release builds. The 32-bit build exists so that 32-bit vendor
J2534 DLLs (XHorse / clone MVCI adapters) can be loaded; a DLL can only be loaded by a
process of the same bitness.

## Layout

- All C++ code, Qt forms and resources live under `src\` (`src\connections`, `src\dbc`,
  `src\re`, `src\ui`, `src\icons`, ...). `SavvyCAN.pro` stays in the root and has
  `INCLUDEPATH += src`. This is a personal fork; upstream parity is not a goal.
- The repo root holds only the project file, the two scripts, docs, `help\`, `examples\`
  and the tooling folders listed below. Keep it that way.
- `build-win.cmd` is the only Windows build script. `build-win.cmd` builds both
  architectures, `build-win.cmd x86` / `build-win.cmd x64` one of them.
  `BUILD.md` is the authoritative build and dependency document (Windows x86, Windows
  x64, Linux); the `/build` skill (`.claude\skills\build\SKILL.md`) is the short
  operational version of it. Keep the two in sync.
- `bin\x86\` and `bin\x64\` are the self-contained release folders (exe, Qt DLLs,
  plugins, help, examples). Gitignored.
- `build-win32\` and `build-win64\` hold intermediate build files only. Gitignored.
- The MCP server (`savvycan-mcp`) and the can-signal-finder skill run
  `bin\<arch>\SavvyCAN.exe --cli`. Rule for `arch`: x86 for J2534/passthrucan adapters
  with 32-bit vendor DLLs (XHorse, clone MVCI) and for adapters without a vendor DLL;
  x64 only for 64-bit vendor DLLs. `connection_help` reports which builds exist.
- `src\plugins\passthrucan\` is the patched Qt passthrucan (J2534) plugin, built per
  architecture by `build-win.cmd` into `src\plugins\passthrucan\out\<arch>\`.
- `src\utils\msvc_stdext_shim.h` is force-included (`/FI`) on MSVC by both .pro files:
  Visual Studio 2026's STL removed `stdext::make_checked_array_iterator`, which Qt 5.15
  headers still use.
- `savvycan-mcp\`, `workspace\`, `.claude\skills\` are the reverse-engineering tooling.
- `.github\workflows\build.yml` builds the same x86 and x64 Windows packages in CI.

## Toolchain (local machine)

- Visual Studio 2026 Build Tools: `C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools`
- Qt 5.15.2 MSVC kits, installed with aqtinstall: `C:\Qt\5.15.2\msvc2019` (x86) and
  `C:\Qt\5.15.2\msvc2019_64` (x64)

## Working rules

- Everything the build produces stays inside this repo (`bin\`, `build-win32\`,
  `build-win64\`). Do not build or stage output in temp directories.
- Do not add more scripts; extend `build-win.cmd` or `clean.cmd`.
- A clean build is `clean.cmd` followed by `build-win.cmd`. Do that after touching
  `SavvyCAN.pro`, moving files or changing the toolchain.
- Check `git status` before and after: build output must stay gitignored.
