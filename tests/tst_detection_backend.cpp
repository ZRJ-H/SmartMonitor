#include <QtTest>
#include <QTemporaryDir>
#include <QScopeGuard>
#include <opencv2/videoio.hpp>
#include "../service/detectionworker.h"
class DetectionBackendTests:public QObject{
 Q_OBJECT
private slots:
 void missingRuntimeFallsBackExplicitlyAndStrictModeFails(){
    const auto model=qEnvironmentVariable("SMART_MONITOR_TEST_MODEL");if(model.isEmpty())QSKIP("Model required");
    QTemporaryDir dir;const auto oldMode=qgetenv("SMART_MONITOR_DETECTION_BACKEND"),oldDir=qgetenv("SMART_MONITOR_GPU_DIR");
    const auto restore=qScopeGuard([&]{oldMode.isNull()?qunsetenv("SMART_MONITOR_DETECTION_BACKEND"):qputenv("SMART_MONITOR_DETECTION_BACKEND",oldMode);oldDir.isNull()?qunsetenv("SMART_MONITOR_GPU_DIR"):qputenv("SMART_MONITOR_GPU_DIR",oldDir);});
    qputenv("SMART_MONITOR_GPU_DIR",dir.path().toUtf8());qputenv("SMART_MONITOR_DETECTION_BACKEND","auto");
    DetectionWorker fallback;QVERIFY(fallback.configure(true,model,.5).isEmpty());QVERIFY(fallback.backend().contains("CPU"));QVERIFY(fallback.backend().contains("降级原因"));
    const auto result=fallback.detect(cv::Mat::zeros(100,100,CV_8UC3));QVERIFY2(result.error.isEmpty(),qPrintable(result.error));
    qputenv("SMART_MONITOR_DETECTION_BACKEND","directml");DetectionWorker strict;QVERIFY(!strict.configure(true,model,.5).isEmpty());
 }
 void gpuAndCpuAgreeOnRealPerson(){
    const auto model=qEnvironmentVariable("SMART_MONITOR_TEST_MODEL"),video=qEnvironmentVariable("SMART_MONITOR_TEST_VIDEO");if(model.isEmpty()||video.isEmpty())QSKIP("Real model/video required");
    const auto old=qgetenv("SMART_MONITOR_DETECTION_BACKEND");const auto restore=qScopeGuard([&]{old.isNull()?qunsetenv("SMART_MONITOR_DETECTION_BACKEND"):qputenv("SMART_MONITOR_DETECTION_BACKEND",old);});
    cv::VideoCapture cap(video.toStdString(),cv::CAP_FFMPEG,{cv::CAP_PROP_N_THREADS,2});cv::Mat frame;QVERIFY(cap.read(frame));
    qputenv("SMART_MONITOR_DETECTION_BACKEND","cpu");DetectionWorker cpu;QVERIFY(cpu.configure(true,model,.5).isEmpty());const auto expected=cpu.detect(frame);QVERIFY(expected.error.isEmpty());QVERIFY(expected.found);
    qputenv("SMART_MONITOR_DETECTION_BACKEND","directml");DetectionWorker gpu;const auto error=gpu.configure(true,model,.5);QVERIFY2(error.isEmpty(),qPrintable(error));QVERIFY(gpu.backend().contains("GPU / DirectML"));const auto actual=gpu.detect(frame);QVERIFY(actual.error.isEmpty());QCOMPARE(actual.found,expected.found);QVERIFY(qAbs(actual.confidence-expected.confidence)<.01);QCOMPARE(actual.boxes.size(),expected.boxes.size());
 }
 void initTestCase(){cv::setNumThreads(4);}
};
QTEST_GUILESS_MAIN(DetectionBackendTests)
#include "tst_detection_backend.moc"
