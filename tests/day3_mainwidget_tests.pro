QT += core gui widgets multimedia multimediawidgets sql testlib
CONFIG += testcase console c++17
TEMPLATE = app
TARGET = day3_mainwidget_tests
include(E:/opencv/opencv_qt6.pri)
SOURCES += \
    $$PWD/tst_day3_mainwidget.cpp \
    $$PWD/../manager/windowmanager.cpp \
    $$PWD/../model/channelconfig.cpp \
    $$PWD/../model/video.cpp \
    $$PWD/../model/configmodel.cpp \
    $$PWD/../model/usermodel.cpp \
    $$PWD/../model/videomodel.cpp \
    $$PWD/../service/authpolicy.cpp \
    $$PWD/../service/authsession.cpp \
    $$PWD/../service/capturecadence.cpp \
    $$PWD/../service/frameratelimiter.cpp \
    $$PWD/../service/channelconfigpolicy.cpp \
    $$PWD/../service/channelconfigservice.cpp \
    $$PWD/../service/monitortask.cpp \
    $$PWD/../service/recordingsegment.cpp \
    $$PWD/../service/recordtask.cpp \
    $$PWD/../service/userservice.cpp \
    $$PWD/../service/videoservice.cpp \
    $$PWD/../util/dbconn.cpp \
    $$PWD/../util/verifycodelabel.cpp \
    $$PWD/../view/loginwidget.cpp \
    $$PWD/../view/videolabel.cpp \
    $$PWD/../view/mywidget.cpp

HEADERS += \
    $$PWD/../manager/windowmanager.h \
    $$PWD/../model/channelconfig.h \
    $$PWD/../model/video.h \
    $$PWD/../model/configmodel.h \
    $$PWD/../model/usermodel.h \
    $$PWD/../model/videomodel.h \
    $$PWD/../service/authpolicy.h \
    $$PWD/../service/authsession.h \
    $$PWD/../service/capturecadence.h \
    $$PWD/../service/frameratelimiter.h \
    $$PWD/../service/channelconfigpolicy.h \
    $$PWD/../service/channelconfigservice.h \
    $$PWD/../service/monitortask.h \
    $$PWD/../service/recordingsegment.h \
    $$PWD/../service/recordtask.h \
    $$PWD/../service/userservice.h \
    $$PWD/../service/videoservice.h \
    $$PWD/../util/dbconn.h \
    $$PWD/../util/verifycodelabel.h \
    $$PWD/../view/loginwidget.h \
    $$PWD/../view/videolabel.h \
    $$PWD/../view/mainwidget.h \
    $$PWD/../view/mywidget.h


include($$PWD/../day7_day8.pri)

HEADERS += ../view/apptheme.h
RESOURCES += ../resources.qrc

include($$PWD/../view/mainwidget.pri)
