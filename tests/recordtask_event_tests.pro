QT += core gui testlib
CONFIG += testcase console c++17
TEMPLATE = app
TARGET = recordtask_event_tests
include(E:/opencv/opencv_qt6.pri)
SOURCES += tst_recordtask_event.cpp ../service/recordtask.cpp ../service/recordingsegment.cpp ../service/frameratelimiter.cpp ../model/video.cpp
HEADERS += ../service/recordtask.h ../service/recordingsegment.h ../service/eventrecordingstate.h
