QT += core testlib
CONFIG += testcase console c++17
TEMPLATE = app
TARGET = day3_monitor_task_tests

include(E:/opencv/opencv_qt6.pri)
INCLUDEPATH += $$PWD/..

SOURCES += \
    $$PWD/tst_day3_monitor_task.cpp \
    $$PWD/../model/channelconfig.cpp \
    $$PWD/../service/capturecadence.cpp \
    $$PWD/../service/frameratelimiter.cpp \
    $$PWD/../service/monitortask.cpp

HEADERS += \
    $$PWD/../model/channelconfig.h \
    $$PWD/../service/capturecadence.h \
    $$PWD/../service/frameratelimiter.h \
    $$PWD/../service/monitortask.h

DESTDIR = $$OUT_PWD/bin

win32 {
    OPENCV_RUNTIME_DLL = E:/opencv/install/x64/mingw/bin/libopencv_world4140.dll
    OPENCV_VIDEOIO_FFMPEG_DLL = E:/opencv/install/x64/mingw/bin/opencv_videoio_ffmpeg4140_64.dll
    QMAKE_POST_LINK += $$QMAKE_COPY $$shell_path($$OPENCV_RUNTIME_DLL) $$shell_path($$DESTDIR)
    QMAKE_POST_LINK += $$escape_expand(\n\t) $$QMAKE_COPY $$shell_path($$OPENCV_VIDEOIO_FFMPEG_DLL) $$shell_path($$DESTDIR)
}
