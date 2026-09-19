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
    void fileSourceLoopsAfterLastFrame();
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

QTEST_GUILESS_MAIN(Day3MonitorTaskTests)
#include "tst_day3_monitor_task.moc"
