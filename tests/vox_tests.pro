QT += core gui quick quickcontrols2 testlib multimedia network dbus
CONFIG += c++20 testcase
TARGET = vox_tests
TEMPLATE = app

INCLUDEPATH += ../src

DEFINES += SRCDIR=\\\"$$PWD/\\\"

HEADERS += fakeaudio.h ../src/audiosystem.h ../src/backend.h ../src/chain.h ../src/config.h ../src/meter.h \
           ../src/pipewireaudio.h ../src/player.h ../src/take.h ../src/testrecorder.h ../src/theme.h ../src/translator.h ../src/instance.h ../src/trayicon.h ../src/windowcontroller.h
SOURCES += vox_tests.cpp ../src/backend.cpp ../src/chain.cpp ../src/config.cpp ../src/meter.cpp \
           ../src/pipewireaudio.cpp ../src/player.cpp ../src/take.cpp ../src/testrecorder.cpp ../src/theme.cpp ../src/translator.cpp ../src/instance.cpp ../src/trayicon.cpp ../src/windowcontroller.cpp
