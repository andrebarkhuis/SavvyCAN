# Patched Qt passthrucan (J2534) plugin

Source: Qt 5.15.2 `qtserialbus/src/plugins/canbus/passthrucan` (LGPL v3 / GPL v2+, see file headers).

Local change (`passthrucanio.cpp`, `connectFlags()`): the CAN channel is opened with
connect flags `0` (standard 11-bit IDs) instead of `CAN29BitID | CANIDBoth`.
Clone J2534 adapters such as the XHorse MVCI accept the upstream flags but then never
return any frames; with flags `0` they work (verified on a Toyota at 500 kbit).

Override at runtime with the environment variable `QT_PASSTHRUCAN_CONNECT_FLAGS`
(decimal or `0x` hex): `0x900` restores upstream behaviour, `0x100` is 29-bit only.

`build-plugin.cmd x86|x64` builds the plugin with the matching MSVC toolchain into
`out\<arch>\` and installs it into that Qt kit's `plugins\canbus` (the stock DLL is kept
as `.dll.orig`) so `windeployqt` deploys the patched one. The repo-root `build-win.cmd`
runs it automatically for the architecture being built.

A J2534 vendor DLL can only be loaded by a process of the same bitness: the XHorse DLL
is 32-bit, so use the x86 build (`bin\x86\SavvyCAN.exe`) with it.
