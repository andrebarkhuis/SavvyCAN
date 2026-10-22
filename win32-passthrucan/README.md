# Patched Qt passthrucan (J2534) plugin for the 32-bit build

Source: Qt 5.15.2 `qtserialbus/src/plugins/canbus/passthrucan` (LGPL v3 / GPL v2+, see file headers).

Local change (`passthrucanio.cpp`, `connectFlags()`): the CAN channel is opened with
connect flags `0` (standard 11-bit IDs) instead of `CAN29BitID | CANIDBoth`.
Clone J2534 adapters such as the XHorse MVCI accept the upstream flags but then never
return any frames; with flags `0` they work (verified on a Toyota at 500 kbit).

Override at runtime with the environment variable `QT_PASSTHRUCAN_CONNECT_FLAGS`
(decimal or `0x` hex): `0x900` restores upstream behaviour, `0x100` is 29-bit only.

`build-plugin.cmd` builds the plugin with the 32-bit MSVC toolchain and installs it into
`C:\Qt\5.15.2\msvc2019\plugins\canbus` (the stock DLL is kept as `.dll.orig`) and into
`..\build-win32\deploy\canbus`. `..\build-win32.cmd` runs it automatically.
