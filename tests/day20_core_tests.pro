QT += core testlib
CONFIG += testcase console c++17
TEMPLATE = app
TARGET = day20_core_tests

INCLUDEPATH += ..

SOURCES += \
    tst_day20_core.cpp \
    ../model/video.cpp \
    ../service/recordingsegment.cpp

HEADERS += \
    ../model/video.h \
    ../service/recordingsegment.h

DESTDIR = ../bin/tests
