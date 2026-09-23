QT += core gui sql testlib
CONFIG += testcase console c++17
TEMPLATE = app
TARGET = day7_day8_regression_tests
INCLUDEPATH += ..

SOURCES += \
    tst_day7_day8_regressions.cpp \
    ../model/featureimage.cpp \
    ../model/imagemodel.cpp \
    ../model/operationlogmodel.cpp \
    ../service/imageservice.cpp \
    ../service/operationlogservice.cpp

HEADERS += \
    ../model/featureimage.h \
    ../model/imagemodel.h \
    ../model/operationlogmodel.h \
    ../service/imageservice.h \
    ../service/operationlogservice.h

DESTDIR = ../bin/tests
