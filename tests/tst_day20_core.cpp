// 历史录像核心测试：描述 Video 元数据和十秒录制分段的预期行为。
// 注意：该文件引用的 video.h 与 recordingsegment.h 当前不在本项目源码目录中。
#include <QtTest>

#include "../model/video.h"
#include "../service/recordingsegment.h"

class Day20CoreTests : public QObject
{
    Q_OBJECT

private slots:
    // 验证 Video 对象完整保留构造时传入的录像元数据。
    void videoPreservesMetadata();
    // 验证分段文件名同时包含开始时间和通道编号。
    void segmentNameContainsTimeAndChannel();
    // 验证录制分段达到十秒后触发滚动。
    void segmentRollsAtTenSeconds();
    // 验证结束分段时生成可持久化的 Video 对象。
    void segmentFinishBuildsVideo();
};

void Day20CoreTests::videoPreservesMetadata()
{
    const QDateTime start(QDate(2026, 9, 16), QTime(12, 1, 2));
    const QDateTime end = start.addSecs(10);
    const Video video("record.mp4", "C:/records/record.mp4", 3,
                      start, end, 4096, 7);

    QCOMPARE(video.getId(), 7);
    QCOMPARE(video.getVideoName(), QString("record.mp4"));
    QCOMPARE(video.getVideoPath(), QString("C:/records/record.mp4"));
    QCOMPARE(video.getChannelId(), 3);
    QCOMPARE(video.getStartTime(), start);
    QCOMPARE(video.getEndTime(), end);
    QCOMPARE(video.getFileSize(), qint64(4096));
}

void Day20CoreTests::segmentNameContainsTimeAndChannel()
{
    const QDateTime start(QDate(2026, 9, 16), QTime(12, 1, 2));
    RecordingSegment segment;
    segment.begin("C:/records", 2, start);

    QCOMPARE(segment.fileName(), QString("Record_20260916_120102_2.mp4"));
    QCOMPARE(segment.filePath(),
             QString("C:/records/Record_20260916_120102_2.mp4"));
}

void Day20CoreTests::segmentRollsAtTenSeconds()
{
    RecordingSegment segment(10000);
    segment.begin("C:/records", 1, QDateTime::currentDateTime());

    QVERIFY(!segment.shouldRoll(9999));
    QVERIFY(segment.shouldRoll(10000));
}

void Day20CoreTests::segmentFinishBuildsVideo()
{
    const QDateTime start(QDate(2026, 9, 16), QTime(12, 1, 2));
    const QDateTime end = start.addSecs(6);
    RecordingSegment segment;
    segment.begin("C:/records", 4, start);

    const Video video = segment.finish(end, 8192);

    QCOMPARE(video.getVideoName(), QString("Record_20260916_120102_4.mp4"));
    QCOMPARE(video.getChannelId(), 4);
    QCOMPARE(video.getStartTime(), start);
    QCOMPARE(video.getEndTime(), end);
    QCOMPARE(video.getFileSize(), qint64(8192));
    QVERIFY(!segment.isActive());
}

QTEST_APPLESS_MAIN(Day20CoreTests)
#include "tst_day20_core.moc"
