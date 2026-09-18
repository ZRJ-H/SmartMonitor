QT += core testlib
CONFIG += testcase console c++17
TEMPLATE = app
TARGET = day2_auth_tests

INCLUDEPATH += ..

SOURCES += \
    tst_day2_auth.cpp \
    ../service/authpolicy.cpp \
    ../service/authsession.cpp

HEADERS += \
    ../service/authpolicy.h \
    ../service/authsession.h

DESTDIR = ../bin/tests
