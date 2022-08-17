QT -= gui
QT += core
QT += network
QT += sql

CONFIG += c++17
CONFIG += console
CONFIG -= app_bundle

DEFINES += QT_DEPRECATED_WARNINGS

SOURCES += \
        commands.cpp \
        main.cpp \
        mysocket.cpp \
        server.cpp

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

HEADERS += \
    actions.h \
    commands.h \
    mysocket.h \
    server.h
