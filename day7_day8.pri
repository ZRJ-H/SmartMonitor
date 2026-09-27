SOURCES += \
    $$PWD/model/featureimage.cpp \
    $$PWD/model/imagemodel.cpp \
    $$PWD/model/systemconfigmodel.cpp \
    $$PWD/model/operationlogmodel.cpp \
    $$PWD/service/imageservice.cpp \
    $$PWD/service/systemconfigservice.cpp \
    $$PWD/service/operationlogservice.cpp

HEADERS += \
    $$PWD/model/featureimage.h \
    $$PWD/model/imagemodel.h \
    $$PWD/model/systemconfigmodel.h \
    $$PWD/model/operationlogmodel.h \
    $$PWD/service/imageservice.h \
    $$PWD/service/systemconfigservice.h \
    $$PWD/service/operationlogservice.h

SOURCES += $$PWD/view/imagepage.cpp
HEADERS += $$PWD/view/imagepage.h

SOURCES += $$PWD/view/operationlogpage.cpp
HEADERS += $$PWD/view/operationlogpage.h

SOURCES += $$PWD/service/eventservice.cpp
HEADERS += $$PWD/service/eventservice.h $$PWD/service/eventrecordingstate.h

SOURCES += $$PWD/service/mediarecoveryservice.cpp
HEADERS += $$PWD/service/mediarecoveryservice.h

SOURCES += $$PWD/service/backupservice.cpp
HEADERS += $$PWD/service/backupservice.h

SOURCES += $$PWD/service/storageworker.cpp $$PWD/service/sourceanalysisworker.cpp
HEADERS += $$PWD/service/storageworker.h $$PWD/service/sourceanalysisworker.h

HEADERS += $$PWD/service/sourceeventrow.h
