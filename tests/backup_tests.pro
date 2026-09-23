QT += core sql testlib
CONFIG += testcase console c++17
TEMPLATE = app
TARGET = backup_tests
SOURCES += tst_backup.cpp ../service/backupservice.cpp
HEADERS += ../service/backupservice.h
