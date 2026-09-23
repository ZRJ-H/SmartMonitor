QT += core sql testlib
CONFIG += testcase console c++17
TEMPLATE = app
TARGET = video_service_tests
SOURCES += tst_video_service.cpp ../model/video.cpp ../model/videomodel.cpp ../service/videoservice.cpp ../util/dbconn.cpp
HEADERS += ../model/video.h ../model/videomodel.h ../service/videoservice.h ../util/dbconn.h
