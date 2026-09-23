QT += core gui sql testlib
CONFIG += testcase console c++17
TEMPLATE = app
TARGET = mysql_integration
include(E:/opencv/opencv_qt6.pri)
SOURCES += tst_mysql_integration.cpp ../service/eventservice.cpp ../service/imageservice.cpp ../service/backupservice.cpp ../model/imagemodel.cpp ../model/featureimage.cpp ../model/video.cpp ../util/dbconn.cpp
HEADERS += ../service/backupservice.h
