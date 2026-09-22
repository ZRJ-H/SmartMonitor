QT += core gui widgets testlib
CONFIG += testcase console c++17
TEMPLATE = app
TARGET = preview_performance_tests

INCLUDEPATH += $$PWD/..

SOURCES += \
    $$PWD/tst_preview_performance.cpp \
    $$PWD/../service/capturecadence.cpp \
    $$PWD/../service/frameratelimiter.cpp \
    $$PWD/../view/videolabel.cpp

HEADERS += \
    $$PWD/../service/capturecadence.h \
    $$PWD/../service/frameratelimiter.h \
    $$PWD/../view/videolabel.h

DESTDIR = $$OUT_PWD/bin
