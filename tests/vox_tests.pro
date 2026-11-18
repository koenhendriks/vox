QT += core gui quick quickcontrols2 testlib
CONFIG += c++20 testcase
TARGET = vox_tests
TEMPLATE = app

INCLUDEPATH += ../src

DEFINES += SRCDIR=\\\"$$PWD/\\\"

HEADERS += fakeaudio.h ../src/audiosystem.h ../src/backend.h ../src/chain.h ../src/config.h ../src/meter.h \
           ../src/pipewireaudio.h ../src/theme.h ../src/translator.h
SOURCES += vox_tests.cpp ../src/backend.cpp ../src/chain.cpp ../src/config.cpp ../src/meter.cpp \
           ../src/pipewireaudio.cpp ../src/theme.cpp ../src/translator.cpp
