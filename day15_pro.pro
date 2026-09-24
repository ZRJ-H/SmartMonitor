#-------------------------------------------------
# Day15 MVC 登录与视频分页案例
#-------------------------------------------------

QT += core gui widgets multimedia multimediawidgets sql

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
    model/video.cpp \
    model/configmodel.cpp \
    model/usermodel.cpp \
    model/videomodel.cpp \
    service/authpolicy.cpp \
    service/authsession.cpp \
    service/capturecadence.cpp \
    service/frameratelimiter.cpp \
    service/channelconfigpolicy.cpp \
    service/channelconfigservice.cpp \
    service/monitortask.cpp \
    service/recordingsegment.cpp \
    service/recordtask.cpp \
    service/userservice.cpp \
    service/videoservice.cpp \
    util/dbconn.cpp \
    util/verifycodelabel.cpp \
    view/loginwidget.cpp \
    view/videolabel.cpp \
    view/mainwidget.cpp \
    view/mywidget.cpp

HEADERS += \
    service/latestframebuffer.h \
    manager/windowmanager.h \
    model/channelconfig.h \
    model/video.h \
    model/configmodel.h \
    model/usermodel.h \
    model/videomodel.h \
    service/authpolicy.h \
    service/authsession.h \
    service/capturecadence.h \
    service/frameratelimiter.h \
    service/channelconfigpolicy.h \
    service/channelconfigservice.h \
    service/monitortask.h \
    service/recordingsegment.h \
    service/recordtask.h \
    service/userservice.h \
    service/videoservice.h \
    util/dbconn.h \
    util/verifycodelabel.h \
    view/loginwidget.h \
    view/videolabel.h \
    view/mainwidget.h \
    view/mywidget.h

qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

isEmpty(DESTDIR): DESTDIR = $$PWD/bin

win32 {
    # 本机独立下载的 Cisco 编码库；允许工作流指定原工程的运行库位置。
    isEmpty(OPENH264_RUNTIME_DLL): OPENH264_RUNTIME_DLL = $$PWD/bin/openh264-2.5.0-win64.dll
    OPENH264_TARGET = $$clean_path($$absolute_path($$DESTDIR)/openh264-2.5.0-win64.dll)
    OPENH264_SOURCE = $$clean_path($$absolute_path($$OPENH264_RUNTIME_DLL))
    exists($$OPENH264_SOURCE):!equals(OPENH264_SOURCE, $$OPENH264_TARGET) {
        QMAKE_POST_LINK += $$QMAKE_COPY $$shell_quote($$shell_path($$OPENH264_SOURCE)) $$shell_quote($$shell_path($$DESTDIR))
        QMAKE_POST_LINK += $$escape_expand(\n\t)
    }
    OPENCV_RUNTIME_DLL = E:/opencv/install/x64/mingw/bin/libopencv_world4140.dll
    OPENCV_VIDEOIO_FFMPEG_DLL = E:/opencv/install/x64/mingw/bin/opencv_videoio_ffmpeg4140_64.dll
    QMAKE_POST_LINK += $$QMAKE_COPY $$shell_path($$OPENCV_RUNTIME_DLL) $$shell_path($$DESTDIR)
    QMAKE_POST_LINK += $$escape_expand(\n\t) $$QMAKE_COPY $$shell_path($$OPENCV_VIDEOIO_FFMPEG_DLL) $$shell_path($$DESTDIR)
}

RESOURCES += resources.qrc

include($$PWD/day7_day8.pri)

HEADERS += view/apptheme.h service/detectionworker.h
