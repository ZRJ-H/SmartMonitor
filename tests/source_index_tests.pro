QT += core gui sql testlib
CONFIG += testcase console c++17
TEMPLATE = app
TARGET = source_index_tests
include(E:/opencv/opencv_qt6.pri)
SOURCES += tst_source_index.cpp ../service/sourceanalysisworker.cpp
HEADERS += ../service/sourceanalysisworker.h ../service/detectionworker.h ../service/ortgpusession.h
