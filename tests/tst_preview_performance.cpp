#include <QtTest>

#include "../service/capturecadence.h"
#include "../service/frameratelimiter.h"
#include "../view/videolabel.h"

#include <QGridLayout>
#include <QPixmap>
#include <QWidget>

class PreviewPerformanceTests : public QObject
{
    Q_OBJECT
private slots:
    void pixmapDoesNotChangeEqualGridCells();
    void limiterCapsEachChannelIndependently();
    void captureCadenceCapsDecodeWorkAndKeepsTimeline();
};

void PreviewPerformanceTests::pixmapDoesNotChangeEqualGridCells()
{
    QWidget container;
    QGridLayout layout(&container);
    VideoLabel first;
    VideoLabel second;
    VideoLabel third;
    VideoLabel fourth;
    layout.addWidget(&first, 0, 0);
    layout.addWidget(&second, 0, 1);
    layout.addWidget(&third, 1, 0);
    layout.addWidget(&fourth, 1, 1);
    layout.setRowStretch(0, 1);
    layout.setRowStretch(1, 1);
    layout.setColumnStretch(0, 1);
    layout.setColumnStretch(1, 1);
    container.resize(800, 600);
    container.show();
    QCoreApplication::processEvents();

    first.setPixmap(QPixmap(1920, 1080));
    second.setPixmap(QPixmap(1280, 720));
    QCoreApplication::processEvents();

    QVERIFY(qAbs(first.height() - third.height()) <= 1);
    QVERIFY(qAbs(second.width() - first.width()) <= 1);
}

void PreviewPerformanceTests::limiterCapsEachChannelIndependently()
{
    FrameRateLimiter limiter(15);
    QVERIFY(limiter.shouldPass(0, 0));
    QVERIFY(!limiter.shouldPass(0, 10));
    QVERIFY(!limiter.shouldPass(0, 65));
    QVERIFY(limiter.shouldPass(0, 67));
    QVERIFY(limiter.shouldPass(1, 10));
}

void PreviewPerformanceTests::captureCadenceCapsDecodeWorkAndKeepsTimeline()
{
    CaptureCadence sixtyFps(60.0, 12.0);
    QCOMPARE(sixtyFps.outputFps(), 12.0);
    QCOMPARE(sixtyFps.timerIntervalMs(), 83);
    QCOMPARE(sixtyFps.framesToAdvance(), 5);
    QCOMPARE(sixtyFps.framesToAdvance(), 5);

    CaptureCadence lowFps(10.0, 12.0);
    QCOMPARE(lowFps.outputFps(), 10.0);
    QCOMPARE(lowFps.timerIntervalMs(), 100);
    QCOMPARE(lowFps.framesToAdvance(), 1);
}

QTEST_MAIN(PreviewPerformanceTests)
#include "tst_preview_performance.moc"
