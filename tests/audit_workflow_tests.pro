QT += core sql testlib
CONFIG += testcase console c++17
TEMPLATE = app
TARGET = audit_workflow_tests
SOURCES += tst_audit_workflow.cpp ../model/operationlogmodel.cpp ../service/operationlogservice.cpp
HEADERS += ../model/operationlogmodel.h ../service/operationlogservice.h
