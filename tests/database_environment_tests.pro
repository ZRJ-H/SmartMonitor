QT += core sql testlib
CONFIG += testcase console c++17
TEMPLATE = app
TARGET = database_environment_tests
SOURCES += tst_database_environment.cpp ../util/dbconn.cpp ../model/configmodel.cpp
HEADERS += ../util/dbconn.h ../model/configmodel.h
