#include <QtTest>

#include "../model/video.h"
#include "../service/recordingsegment.h"

class Day20CoreTests : public QObject
{
    Q_OBJECT

private slots:
    void videoPreservesMetadata();
    void segmentNameContainsTimeAndChannel();
    void segmentRollsAtTenSeconds();
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
