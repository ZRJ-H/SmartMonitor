QT += core gui sql testlib
CONFIG += testcase console c++17
TEMPLATE = app
TARGET = media_recovery_tests
SOURCES += tst_media_recovery.cpp ../service/mediarecoveryservice.cpp ../service/eventservice.cpp ../service/videoservice.cpp ../service/imageservice.cpp ../model/videomodel.cpp ../model/video.cpp ../model/imagemodel.cpp ../model/featureimage.cpp ../util/dbconn.cpp
HEADERS += ../service/mediarecoveryservice.h
