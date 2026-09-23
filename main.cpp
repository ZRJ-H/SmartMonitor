// 程序入口：创建 Qt 应用，启动窗口管理器，并在事件循环结束后依次释放界面与数据库资源。
#include "manager/windowmanager.h"
#include "util/dbconn.h"

#include <QApplication>
#include <QTimer>
#include <QSqlDatabase>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName("QtProjects");
    QCoreApplication::setApplicationName("SmartMonitor");
    const bool smoke = app.arguments().contains("--smoke-test");
    if (smoke) {
        qputenv("SMART_MONITOR_AUTOSTART", "0");
        if (!DbConn::getInstance()->database().isOpen()) return 2;
        QTimer::singleShot(2000, &app, &QCoreApplication::quit);
    }

    // QApplication 创建后才首次获取懒汉式界面管理器。
    WindowManager *window_manager = WindowManager::getInstance();
    window_manager->showMainWindow();

    const int result = app.exec();

    // 先删除窗口及其 Model/Service，再移除数据库连接。
    WindowManager::releaseInstance();
    DbConn::releaseDbConn();
    return result;
}
