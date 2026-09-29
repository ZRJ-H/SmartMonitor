#include <QtTest>
#include "../service/resourcemonitor.h"
class ResourceMonitorTests:public QObject {
    Q_OBJECT
private slots:
    void cpuNormalizesAcrossCores(){
        QCOMPARE(ResourceMetrics::cpuPercent(10000000,1000000000,8),12.5);
        QCOMPARE(ResourceMetrics::cpuPercent(80000000,1000000000,8),100.0);
        QCOMPARE(ResourceMetrics::cpuPercent(0,1000000000,8),0.0);
        QCOMPARE(ResourceMetrics::cpuPercent(10,0,8),-1.0);
    }
    void gpuExcludesOtherProcessesAndUsesBusiestEngine(){
        QList<QPair<QString,double>> engines{
            {"pid_111_luid_0x0_0x123_phys_0_eng_0_engtype_3D",8},
            {"pid_111_luid_0x0_0x123_phys_0_eng_0_engtype_3D#1",4},
            {"pid_111_luid_0x0_0x123_phys_0_eng_1_engtype_Compute",20},
            {"pid_222_luid_0x0_0x123_phys_0_eng_6_engtype_VideoEncode",84}};
        QCOMPARE(ResourceMetrics::gpuPercent(engines,111),20.0);
        QCOMPARE(ResourceMetrics::gpuPercent(engines,333),0.0);
        engines.append({"pid_111_luid_0x0_0x123_phys_0_eng_2_engtype_Copy",-1});
        QCOMPARE(ResourceMetrics::gpuPercent(engines,111),-1.0);
    }
    void unavailableIsNotZero(){
        QCOMPARE(ResourceMetrics::percentText(-1),QStringLiteral("—"));
        QCOMPARE(ResourceMetrics::percentText(0),QString("0.0%"));
        QCOMPARE(ResourceMetrics::percentText(12.34),QString("12.3%"));
    }
    void samplesRealProcessAndStops(){
        ResourceMonitor monitor;QSignalSpy samples(&monitor,&ResourceMonitor::usageReady);
        monitor.start();QTRY_VERIFY_WITH_TIMEOUT(samples.count()>=3,15000);
        const auto values=samples.last();
        qInfo()<<"Process CPU/GPU/memory %:"<<values;
        QVERIFY(values[0].toDouble()>=0 && values[0].toDouble()<=100);
        QVERIFY(values[1].toDouble()==-1 || (values[1].toDouble()>=0 && values[1].toDouble()<=100));
        QVERIFY(values[2].toDouble()>0 && values[2].toDouble()<=100);
        monitor.stop();QVERIFY(!monitor.isRunning());
        const auto count=samples.count();QTest::qWait(1100);QCOMPARE(samples.count(),count);
    }
};
QTEST_GUILESS_MAIN(ResourceMonitorTests)
#include "tst_resource_monitor.moc"