QT += core sql testlib
CONFIG += testcase console c++17
TEMPLATE = app
TARGET = day3_channel_config_tests

INCLUDEPATH += $$PWD/..

SOURCES += \
    $$PWD/tst_day3_channel_config.cpp \
    $$PWD/../model/channelconfig.cpp \
    $$PWD/../model/configmodel.cpp \
    $$PWD/../service/channelconfigpolicy.cpp \
    $$PWD/../service/channelconfigservice.cpp \
    $$PWD/../util/dbconn.cpp

HEADERS += \
    $$PWD/../model/channelconfig.h \
    $$PWD/../model/configmodel.h \
    $$PWD/../service/channelconfigpolicy.h \
    $$PWD/../service/channelconfigservice.h \
    $$PWD/../util/dbconn.h

DESTDIR = $$OUT_PWD/bin
