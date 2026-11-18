QT += core gui qml quick quickcontrols2 multimedia network

CONFIG += c++20 release
TARGET = vox
TEMPLATE = app

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
