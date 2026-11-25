QT += core gui quick quickcontrols2 testlib
CONFIG += c++17 testcase
TARGET = agenda_tests
TEMPLATE = app

INCLUDEPATH += ../src

HEADERS += ../src/backend.h ../src/event.h ../src/format.h ../src/reminders.h ../src/store.h \
    ../src/theme.h ../src/translator.h
SOURCES += agenda_tests.cpp ../src/backend.cpp ../src/event.cpp ../src/format.cpp ../src/reminders.cpp \
    ../src/store.cpp ../src/theme.cpp ../src/translator.cpp
DEFINES += SRCDIR=\\\"$$PWD\\\"
