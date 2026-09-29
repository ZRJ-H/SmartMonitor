# 主程序和集成测试共用组件清单。
SOURCES += $$PWD/../service/resourcemonitor.cpp
HEADERS += $$PWD/../service/resourcemonitor.h
win32:LIBS += -lpdh -lpsapi
SOURCES += $$PWD/mainwidget.cpp $$PWD/playbackpage.cpp $$PWD/monitorpage.cpp $$PWD/settingspage.cpp \
    $$PWD/../service/monitoringcontroller.cpp $$PWD/../service/maintenancecontroller.cpp
HEADERS += $$PWD/playbackpage.h $$PWD/monitorpage.h $$PWD/settingspage.h \
    $$PWD/../service/monitoringcontroller.h $$PWD/../service/maintenancecontroller.h
