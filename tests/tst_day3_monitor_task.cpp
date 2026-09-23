// 视频采集任务测试：验证文件源循环播放，并确保单路打开失败只影响对应通道。
#include <QtTest>

#include "../model/channelconfig.h"
#include "../service/monitortask.h"

#include <QSignalSpy>
#include <QTemporaryDir>

#include <opencv2/core.hpp>
#include <opencv2/videoio.hpp>

class Day3MonitorTaskTests : public QObject
{
    Q_OBJECT

private slots:
    // 验证本地视频到达末帧后会回到开头继续发送画面。
    void fileSourceLoopsAfterLastFrame();
    void mp4FileSourceCanBeOpened();
    // 验证缺失文件的错误信号携带正确通道号，不污染其他通道。
    void missingFileReportsOnlyItsChannel();
};

void Day3MonitorTaskTests::fileSourceLoopsAfterLastFrame()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("loop.avi"));

    cv::VideoWriter writer(
        path.toStdString(),
        cv::VideoWriter::fourcc('M', 'J', 'P', 'G'),
        20.0,
        cv::Size(32, 24)
    );
    QVERIFY(writer.isOpened());
    writer.write(cv::Mat(24, 32, CV_8UC3, cv::Scalar(0, 0, 255)));
    writer.write(cv::Mat(24, 32, CV_8UC3, cv::Scalar(0, 255, 0)));
    writer.release();

    MonitorTask task;
    QSignalSpy frameSpy(&task, &MonitorTask::frameReady);
    QSignalSpy errorSpy(&task, &MonitorTask::errorOccurred);

    ChannelConfig config;
    config.channelId = 2;
    config.name = QStringLiteral("西门");
    config.sourceType = ChannelSourceType::File;
    config.filePath = path;

    task.startCapture(config);
    QTRY_VERIFY_WITH_TIMEOUT(frameSpy.count() >= 3, 1500);
    QCOMPARE(errorSpy.count(), 0);
    task.stopCapture();
}

void Day3MonitorTaskTests::missingFileReportsOnlyItsChannel()
{
    MonitorTask task;
    QSignalSpy errorSpy(&task, &MonitorTask::errorOccurred);

    ChannelConfig config;
    config.channelId = 4;
    config.name = QStringLiteral("机房");
    config.sourceType = ChannelSourceType::File;
    config.filePath = QStringLiteral("C:/not-present/day3-missing.mp4");

    task.startCapture(config);

    QCOMPARE(errorSpy.count(), 1);
    QCOMPARE(errorSpy.at(0).at(0).toInt(), 4);
    QVERIFY(errorSpy.at(0).at(1).toString().contains(QStringLiteral("无法打开")));
}

void Day3MonitorTaskTests::mp4FileSourceCanBeOpened()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("local.mp4"));
    cv::VideoWriter writer(path.toStdString(),
        cv::VideoWriter::fourcc('m', 'p', '4', 'v'), 20.0, cv::Size(32, 24));
    QVERIFY2(writer.isOpened(), "MP4 videoio backend is unavailable");
    writer.write(cv::Mat(24, 32, CV_8UC3, cv::Scalar(255, 0, 0)));
    writer.release();

    MonitorTask task;
    QSignalSpy frameSpy(&task, &MonitorTask::frameReady);
    QSignalSpy errorSpy(&task, &MonitorTask::errorOccurred);
    ChannelConfig config;
    config.channelId = 2;
    config.name = QStringLiteral("本地 MP4");
    config.sourceType = ChannelSourceType::File;
    config.filePath = path;
    QVERIFY(QMetaObject::invokeMethod(&task, "configureCaptureInterval", Q_ARG(int, 500)));
    QSignalSpy ready(&task, &MonitorTask::sourceReady);
    task.startCapture(config);
    QCOMPARE(ready.count(), 1);
    QCOMPARE(ready.first().at(1).toDouble(), 2.0);
    QTRY_VERIFY_WITH_TIMEOUT(frameSpy.count() >= 1, 1500);
    QCOMPARE(errorSpy.count(), 0);
    task.stopCapture();
}

QTEST_GUILESS_MAIN(Day3MonitorTaskTests)
#include "tst_day3_monitor_task.moc"
