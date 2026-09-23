QT += core gui widgets sql testlib
CONFIG += testcase console c++17
TEMPLATE = app
TARGET = day7_day8_ui_tests
INCLUDEPATH += ..

SOURCES += \
    tst_day7_day8_ui.cpp \
    ../model/featureimage.cpp \
    ../model/imagemodel.cpp \
    ../service/imageservice.cpp \
    ../view/imagepage.cpp

HEADERS += \
    ../model/featureimage.h \
    ../model/imagemodel.h \
    ../service/imageservice.h \
    ../view/imagepage.h

DESTDIR = ../bin/tests
