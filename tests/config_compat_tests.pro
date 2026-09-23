QT += core gui sql testlib
CONFIG += testcase console c++17
TEMPLATE = app
TARGET = config_compat_tests
SOURCES += tst_config_compat.cpp ../service/systemconfigservice.cpp ../model/systemconfigmodel.cpp
HEADERS += ../service/systemconfigservice.h ../model/systemconfigmodel.h
