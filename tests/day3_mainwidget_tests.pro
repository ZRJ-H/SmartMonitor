QT += core gui widgets multimedia sql testlib
CONFIG += testcase console c++17
TEMPLATE = app
TARGET = day3_mainwidget_tests

include(E:/opencv/opencv_qt6.pri)
INCLUDEPATH += $$PWD/..

SOURCES += \
    $$PWD/tst_day3_mainwidget.cpp \
    $$PWD/../view/mainwidget.cpp \
    $$PWD/../model/channelconfig.cpp \
    $$PWD/../model/configmodel.cpp \
    $$PWD/../model/videomodel.cpp \
    $$PWD/../service/authpolicy.cpp \
    $$PWD/../service/authsession.cpp \
    $$PWD/../service/channelconfigpolicy.cpp \
    $$PWD/../service/channelconfigservice.cpp \
    $$PWD/../service/monitortask.cpp \
    $$PWD/../service/videoservice.cpp \
    $$PWD/../util/dbconn.cpp

HEADERS += \
    $$PWD/../view/mainwidget.h \
    $$PWD/../model/channelconfig.h \
    $$PWD/../model/configmodel.h \
    $$PWD/../model/videomodel.h \
    $$PWD/../service/authpolicy.h \
    $$PWD/../service/authsession.h \
    $$PWD/../service/channelconfigpolicy.h \
    $$PWD/../service/channelconfigservice.h \
    $$PWD/../service/monitortask.h \
    $$PWD/../service/videoservice.h \
    $$PWD/../util/dbconn.h

DESTDIR = $$OUT_PWD/bin
