QT += core gui qml quick quickcontrols2 multimedia network dbus

CONFIG += c++20 release
TARGET = vox
TEMPLATE = app

# The one place the version is set; release.yml checks the tag against it.
VERSION = 0.3.2

# A define would change only the compiler flags, which make doesn't track, so
# a bumped version would leave main.o stale. A header written only when the
# version changes makes make rebuild what includes it.
VERSION_HEADER = $$OUT_PWD/version.h
VERSION_LINE = "$${LITERAL_HASH}define VOX_VERSION \"$$VERSION\""
!equals(VERSION_LINE, $$cat($$VERSION_HEADER, blob)) {
    !write_file($$VERSION_HEADER, VERSION_LINE): error("Couldn't write $$VERSION_HEADER")
}
INCLUDEPATH += $$OUT_PWD

HEADERS += \
    src/audiosystem.h \
    src/backend.h \
    src/chain.h \
    src/cli.h \
    src/config.h \
    src/instance.h \
    src/meter.h \
    src/pipewireaudio.h \
    src/player.h \
    src/take.h \
    src/testrecorder.h \
    src/theme.h \
    src/translator.h \
    src/trayicon.h \
    src/windowcontroller.h

SOURCES += \
    src/main.cpp \
    src/backend.cpp \
    src/chain.cpp \
    src/cli.cpp \
    src/config.cpp \
    src/instance.cpp \
    src/meter.cpp \
    src/pipewireaudio.cpp \
    src/player.cpp \
    src/take.cpp \
    src/testrecorder.cpp \
    src/theme.cpp \
    src/translator.cpp \
    src/trayicon.cpp \
    src/windowcontroller.cpp

RESOURCES += src/resources.qrc
