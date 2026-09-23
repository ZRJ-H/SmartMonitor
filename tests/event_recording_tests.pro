QT += core gui sql testlib
CONFIG += testcase console c++17
TEMPLATE = app
TARGET = event_recording_tests
SOURCES += tst_event_recording.cpp ../service/eventservice.cpp ../service/imageservice.cpp ../model/imagemodel.cpp ../model/featureimage.cpp ../model/video.cpp
HEADERS += ../service/eventrecordingstate.h ../service/eventservice.h
