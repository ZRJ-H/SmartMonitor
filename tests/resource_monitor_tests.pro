QT += core testlib
QT -= gui
CONFIG += console testcase c++17
TARGET = resource_monitor_tests
TEMPLATE = app
SOURCES += tst_resource_monitor.cpp ../service/resourcemonitor.cpp
HEADERS += ../service/resourcemonitor.h
win32:LIBS += -lpdh -lpsapi