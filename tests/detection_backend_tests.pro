QT += core testlib
CONFIG += testcase console c++17
TEMPLATE = app
TARGET = detection_backend_tests
include(E:/opencv/opencv_qt6.pri)
SOURCES += tst_detection_backend.cpp
HEADERS += ../service/detectionworker.h ../service/ortgpusession.h
