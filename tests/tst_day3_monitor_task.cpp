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
    void fileDecoderThreadsStayBoundedAcrossLoopAndRestart() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath("bounded-single.mp4");
        cv::VideoWriter writer(path.toStdString(), cv::VideoWriter::fourcc('m','p','4','v'),
                               20, cv::Size(64,48));
        QVERIFY2(writer.isOpened(), "MP4 videoio backend is unavailable");
        writer.write(cv::Mat(48,64,CV_8UC3,cv::Scalar(20,40,80)));
        writer.release();

        ChannelConfig config;
        config.channelId=1;config.name="bounded decoder";
        config.sourceType=ChannelSourceType::File;config.filePath=path;
        MonitorTask task;
        QSignalSpy frames(&task,&MonitorTask::frameReady);
        QSignalSpy errors(&task,&MonitorTask::errorOccurred);
        QList<int> reportedThreads;
        connect(&task,&MonitorTask::frameReady,this,[&]() {
            reportedThreads.append(task.decoderThreadCount());
        });
        task.startCapture(config);
        QCOMPARE(task.decoderThreadCount(),2);
        // A single-frame file must cross EOF repeatedly without restoring the default pool.
        QTRY_VERIFY_WITH_TIMEOUT(frames.count()>=5,2000);
        for(int threads:reportedThreads) QCOMPARE(threads,2);
        QCOMPARE(errors.count(),0);
        task.stopCapture();
        QCOMPARE(task.decoderThreadCount(),0);
        const int beforeRestart=frames.count();
        task.startCapture(config);
        QCOMPARE(task.decoderThreadCount(),2);
        QTRY_VERIFY_WITH_TIMEOUT(frames.count()>beforeRestart,1000);
        QCOMPARE(errors.count(),0);
        task.stopCapture();
    }
    void singleFrameVideoLoopsWithoutStoppingChannel() {
        QTemporaryDir dir;const auto path=dir.filePath("single.avi");
        cv::VideoWriter writer(path.toStdString(),cv::VideoWriter::fourcc('M','J','P','G'),20,cv::Size(32,24));
        QVERIFY(writer.isOpened());writer.write(cv::Mat(24,32,CV_8UC3,cv::Scalar(20,40,80)));writer.release();
        ChannelConfig config;config.channelId=1;config.name="single";config.sourceType=ChannelSourceType::File;config.filePath=path;
        MonitorTask task;QSignalSpy frames(&task,&MonitorTask::frameReady);QSignalSpy errors(&task,&MonitorTask::errorOccurred);
        task.startCapture(config);QTRY_VERIFY_WITH_TIMEOUT(frames.count()>=3,1000);
        QCOMPARE(errors.count(),0);task.stopCapture();
    }

    void invalidModelKeepsPreviewAliveAndStopPreventsNewFrames() {
        QTemporaryDir dir;const auto path=dir.filePath("preview.avi");
        cv::VideoWriter writer(path.toStdString(),cv::VideoWriter::fourcc('M','J','P','G'),20,cv::Size(32,24));
        QVERIFY(writer.isOpened());for(int i=0;i<3;++i) writer.write(cv::Mat(24,32,CV_8UC3,cv::Scalar(20,40,80)));writer.release();
        ChannelConfig config;config.channelId=1;config.name="test";config.sourceType=ChannelSourceType::File;config.filePath=path;
        MonitorTask task;QSignalSpy frames(&task,&MonitorTask::frameReady);QSignalSpy messages(&task,&MonitorTask::detectionMessage);
        QSignalSpy errors(&task,&MonitorTask::errorOccurred); task.startCapture(config);task.configureDetection(true,dir.filePath("missing.onnx"),0.5);
        QCOMPARE(messages.count(),1);QVERIFY(messages[0][1].toString().contains(QString::fromUtf8("不存在")));
        QTRY_VERIFY_WITH_TIMEOUT(frames.count()>=2,1000);
        QCOMPARE(errors.count(),0);task.stopCapture();const int stopped=frames.count();QTest::qWait(200);QCOMPARE(frames.count(),stopped);
        task.startCapture(config);QTRY_VERIFY_WITH_TIMEOUT(frames.count()>stopped,1000);task.stopCapture();
    }


    // 回归：25 FPS 文件应按源帧率输出，而不是被内部 12 FPS 上限截断。
    void fileUsesSourceRateWithoutAccumulatingProcessingDelay() {
        QTemporaryDir dir;
        auto path=dir.filePath("cadence.avi");
        cv::VideoWriter writer(path.toStdString(),cv::VideoWriter::fourcc('M','J','P','G'),25,cv::Size(64,48));
        QVERIFY(writer.isOpened());
        for(int i=0;i<100;++i) writer.write(cv::Mat(48,64,CV_8UC3,cv::Scalar(i,40,80)));
        writer.release();
        ChannelConfig config;config.channelId=1;config.sourceType=ChannelSourceType::File;config.filePath=path;
        MonitorTask task;
        QSignalSpy ready(&task,&MonitorTask::sourceReady);
        int frames=0;
        connect(&task,&MonitorTask::frameReady,this,[&]{++frames;QThread::msleep(8);});
        task.startCapture(config);
        QCOMPARE(ready.first()[1].toDouble(),25.0);
        QTest::qWait(1600);task.stopCapture();
        QVERIFY2(frames>=32,qPrintable(QString("Expected >=32 frames, got %1").arg(frames)));
        QVERIFY(frames<=44);
    }
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
    QCOMPARE(ready.first().at(1).toDouble(), 20.0); // 旧采集间隔配置不再覆盖文件 FPS。
    QTRY_VERIFY_WITH_TIMEOUT(frameSpy.count() >= 1, 1500);
    QCOMPARE(errorSpy.count(), 0);
    task.stopCapture();
}

QTEST_GUILESS_MAIN(Day3MonitorTaskTests)
#include "tst_day3_monitor_task.moc"
