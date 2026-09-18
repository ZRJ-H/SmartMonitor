#include "manager/windowmanager.h"
#include "util/dbconn.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    // QApplication 创建后才首次获取懒汉式界面管理器。
    WindowManager *window_manager = WindowManager::getInstance();
    window_manager->showMainWindow();

    const int result = app.exec();

    // 先删除窗口及其 Model/Service，再移除数据库连接。
    WindowManager::releaseInstance();
    DbConn::releaseDbConn();
    return result;
}
