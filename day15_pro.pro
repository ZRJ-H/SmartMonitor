#-------------------------------------------------
# Day15 MVC 登录与视频分页案例
#-------------------------------------------------

QT += core gui widgets multimedia sql

TARGET = day20_smart_monitor
TEMPLATE = app
DEFINES += QT_DEPRECATED_WARNINGS
CONFIG += c++17

# Qt 6.11.2 + MinGW 64 位使用的 OpenCV 4.14 配置。
# 统一 include 可避免重复维护头文件和库路径。
include(E:/opencv/opencv_qt6.pri)

SOURCES += \
    main.cpp \
    manager/windowmanager.cpp \
    model/channelconfig.cpp \
    model/configmodel.cpp \
    model/usermodel.cpp \
    model/videomodel.cpp \
    service/authpolicy.cpp \
    service/authsession.cpp \
    service/channelconfigpolicy.cpp \
    service/channelconfigservice.cpp \
    service/monitortask.cpp \
    service/userservice.cpp \
    service/videoservice.cpp \
    util/dbconn.cpp \
    util/verifycodelabel.cpp \
    view/loginwidget.cpp \
    view/mainwidget.cpp \
    view/mywidget.cpp

HEADERS += \
    manager/windowmanager.h \
    model/channelconfig.h \
    model/configmodel.h \
    model/usermodel.h \
    model/videomodel.h \
    service/authpolicy.h \
    service/authsession.h \
    service/channelconfigpolicy.h \
    service/channelconfigservice.h \
    service/monitortask.h \
    service/userservice.h \
    service/videoservice.h \
    util/dbconn.h \
    util/verifycodelabel.h \
    view/loginwidget.h \
    view/mainwidget.h \
    view/mywidget.h

qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

DESTDIR = $$PWD/bin

RESOURCES += resources.qrc
