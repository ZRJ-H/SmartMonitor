QT += core gui widgets sql testlib
CONFIG += testcase console c++17
TEMPLATE = app
TARGET = ui_theme_tests
SOURCES += tst_ui_theme.cpp \
    ../view/loginwidget.cpp \
    ../util/verifycodelabel.cpp \
    ../service/userservice.cpp \
    ../service/authpolicy.cpp \
    ../model/usermodel.cpp \
    ../util/dbconn.cpp
HEADERS += ../view/apptheme.h ../view/loginwidget.h ../util/verifycodelabel.h
RESOURCES += ../resources.qrc