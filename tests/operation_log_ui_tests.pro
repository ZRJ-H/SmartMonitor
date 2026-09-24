QT += core gui widgets sql testlib
CONFIG += testcase console c++17
TEMPLATE = app
TARGET = operation_log_ui_tests
SOURCES += $$PWD/tst_operation_log_ui.cpp \
    $$PWD/../view/operationlogpage.cpp \
    $$PWD/../model/operationlogmodel.cpp \
    $$PWD/../service/operationlogservice.cpp
HEADERS += $$PWD/../view/operationlogpage.h \
    $$PWD/../view/apptheme.h \
    $$PWD/../model/operationlogmodel.h \
    $$PWD/../service/operationlogservice.h
RESOURCES += $$PWD/../resources.qrc
