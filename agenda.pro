QT += core gui qml quick quickcontrols2

CONFIG += c++17 release
TARGET = agenda
TEMPLATE = app

HEADERS += \
    src/backend.h \
    src/cli.h \
    src/event.h \
    src/format.h \
    src/reminders.h \
    src/store.h \
    src/theme.h \
    src/translator.h

SOURCES += \
    src/main.cpp \
    src/backend.cpp \
    src/cli.cpp \
    src/event.cpp \
    src/format.cpp \
    src/reminders.cpp \
    src/store.cpp \
    src/theme.cpp \
    src/translator.cpp

RESOURCES += src/resources.qrc
