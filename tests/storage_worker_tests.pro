QT += core gui sql testlib
CONFIG += testcase console c++17
TEMPLATE = app
TARGET = storage_worker_tests
SOURCES += tst_storage_worker.cpp ../service/storageworker.cpp ../service/mediarecoveryservice.cpp ../service/eventservice.cpp ../service/imageservice.cpp ../service/videoservice.cpp ../service/operationlogservice.cpp ../model/video.cpp ../model/videomodel.cpp ../model/featureimage.cpp ../model/imagemodel.cpp ../model/operationlogmodel.cpp ../util/dbconn.cpp
HEADERS += ../service/storageworker.h ../util/dbconn.h
