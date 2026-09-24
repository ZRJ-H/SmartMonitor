QT += core testlib
QT -= gui
CONFIG += testcase console c++17
TEMPLATE = app
TARGET = frame_buffer_tests

include(E:/opencv/opencv_qt6.pri)
INCLUDEPATH += $$PWD/..

SOURCES += $$PWD/tst_frame_buffer.cpp
HEADERS += $$PWD/../service/latestframebuffer.h
DESTDIR = $$OUT_PWD/bin

win32 {
    OPENCV_RUNTIME_DLL = E:/opencv/install/x64/mingw/bin/libopencv_world4140.dll
    QMAKE_POST_LINK += $$QMAKE_COPY $$shell_path($$OPENCV_RUNTIME_DLL) $$shell_path($$DESTDIR)
}
