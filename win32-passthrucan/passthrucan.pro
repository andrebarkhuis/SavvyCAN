# Standalone build of Qt 5.15.2's "passthrucan" (J2534) plugin with a local patch:
# the CAN channel is opened with configurable connect flags (default: 11-bit IDs only),
# because clone J2534 adapters such as the XHorse MVCI return no frames when the
# channel is opened with CAN_ID_BOTH as the stock plugin does.
QT = core serialbus
TEMPLATE = lib
CONFIG += plugin c++17
CONFIG(release, debug|release):DEFINES += QT_NO_DEBUG_OUTPUT
TARGET = qtpassthrucanbus
DESTDIR = $$PWD/out

SOURCES += j2534passthru.cpp main.cpp passthrucanio.cpp passthrucanbackend.cpp
HEADERS += j2534passthru.h passthrucanio.h passthrucanbackend.h
DISTFILES += plugin.json
