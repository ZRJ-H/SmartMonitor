QT += core testlib
CONFIG += testcase console c++17
TEMPLATE = app
TARGET = day3_monitor_task_tests

include(E:/opencv/opencv_qt6.pri)
INCLUDEPATH += $$PWD/..

SOURCES += \
    $$PWD/tst_day3_monitor_task.cpp \
    $$PWD/../model/channelconfig.cpp \
    $$PWD/../service/monitortask.cpp

HEADERS += \
    $$PWD/../model/channelconfig.h \
    $$PWD/../service/monitortask.h

DESTDIR = $$OUT_PWD/bin
