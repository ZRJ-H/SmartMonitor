// 【阅读导引】
// 应用入口：设置运行环境、建立窗口并进入 Qt 事件循环；窗口释放后才移除数据库连接。

// 程序入口：创建 Qt 应用，启动窗口管理器，并在事件循环结束后依次释放界面与数据库资源。
#include "manager/windowmanager.h"
#include "util/dbconn.h"
#include "service/detectionworker.h"

#include <cstdio>
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
    const int gpuCheck=app.arguments().indexOf("--gpu-check");
    if(gpuCheck>=0){
        if(gpuCheck+1>=app.arguments().size()){qCritical("GPU check requires an ONNX model path");return 4;}
        qputenv("SMART_MONITOR_DETECTION_BACKEND","directml");
        DetectionWorker detector;const QString error=detector.configure(true,app.arguments().at(gpuCheck+1),0.5);
        if(!error.isEmpty()){qCritical().noquote()<<error;return 5;}
        const auto result=detector.detect(cv::Mat::zeros(640,640,CV_8UC3));
        if(!result.error.isEmpty()){qCritical().noquote()<<result.error;return 6;}
        std::fprintf(stdout,"GPU_INFERENCE_OK %s\n",detector.backend().toUtf8().constData());std::fflush(stdout);return 0;
    }
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
        std::fprintf(stdout,"RUNTIME_DRIVERS_OK\n");std::fflush(stdout);
        return 0; // 检查成功即退出，不能继续进入业务窗口和自动采集。
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
