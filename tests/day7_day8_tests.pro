QT += core gui sql testlib
CONFIG += testcase console c++17
TEMPLATE = app
TARGET = day7_day8_tests

INCLUDEPATH += ..

SOURCES += \
    tst_day7_day8.cpp \
    ../model/featureimage.cpp \
    ../model/imagemodel.cpp \
    ../model/systemconfigmodel.cpp \
    ../model/operationlogmodel.cpp \
    ../service/imageservice.cpp \
    ../service/systemconfigservice.cpp \
    ../service/operationlogservice.cpp

HEADERS += \
    ../model/featureimage.h \
    ../model/imagemodel.h \
    ../model/systemconfigmodel.h \
    ../model/operationlogmodel.h \
    ../service/imageservice.h \
    ../service/systemconfigservice.h \
    ../service/operationlogservice.h

DESTDIR = ../bin/tests
