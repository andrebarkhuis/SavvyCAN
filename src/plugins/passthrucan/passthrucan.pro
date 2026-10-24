# Standalone build of Qt 5.15.2's "passthrucan" (J2534) plugin with a local patch:
# the CAN channel is opened with configurable connect flags (default: 11-bit IDs only),
# because clone J2534 adapters such as the XHorse MVCI return no frames when the
# channel is opened with CAN_ID_BOTH as the stock plugin does.
QT = core serialbus
TEMPLATE = lib
CONFIG += plugin c++17
win32-msvc: QMAKE_CXXFLAGS += /FI$$shell_quote($$PWD/../../utils/msvc_stdext_shim.h)
CONFIG(release, debug|release):DEFINES += QT_NO_DEBUG_OUTPUT
TARGET = qtpassthrucanbus
isEmpty(ARCH): ARCH = x86   # x86 or x64, passed by build-plugin.cmd
DESTDIR = $$PWD/out/$$ARCH

SOURCES += j2534passthru.cpp main.cpp passthrucanio.cpp passthrucanbackend.cpp
HEADERS += j2534passthru.h passthrucanio.h passthrucanbackend.h
DISTFILES += plugin.json
