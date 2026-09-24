// 【阅读导引】
// 应用入口：设置运行环境、建立窗口并进入 Qt 事件循环；窗口释放后才移除数据库连接。

// 程序入口：创建 Qt 应用，启动窗口管理器，并在事件循环结束后依次释放界面与数据库资源。
#include "manager/windowmanager.h"
#include "util/dbconn.h"

#include <QApplication>
#include <QTimer>
#include <QSqlDatabase>
#include <QThread>
#include <opencv2/core.hpp>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    // 避免 DNN 和图像运算占满所有逻辑核心；在启动任何工作线程前设置。
    cv::setNumThreads(qBound(1, QThread::idealThreadCount(), 4));
    QCoreApplication::setOrganizationName("QtProjects");
    QCoreApplication::setApplicationName("SmartMonitor");
    const bool smoke = app.arguments().contains("--smoke-test");
    if (app.arguments().contains("--runtime-check")) {
        // Load both deployed SQL plugins without accessing the production database.
        for (const QString &driver : {QStringLiteral("QSQLITE"), QStringLiteral("QMYSQL")}) {
            bool valid = false;
            {
                auto probe = QSqlDatabase::addDatabase(driver, QStringLiteral("runtime-probe"));
                valid = probe.isValid();
            }
            QSqlDatabase::removeDatabase(QStringLiteral("runtime-probe"));
            if (!valid) return 3;
        }
        qInfo("RUNTIME_DRIVERS_OK");
    }
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
