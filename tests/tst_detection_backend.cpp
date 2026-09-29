#include <QtTest>
#include <QTemporaryDir>
#include <QScopeGuard>
#include <opencv2/videoio.hpp>
#include "../service/detectionworker.h"
class DetectionBackendTests:public QObject{
 Q_OBJECT
private slots:
 void hotSwitchesSameWorkerAndKeepsSessionOnFailure(){
    const auto model=qEnvironmentVariable("SMART_MONITOR_TEST_MODEL");if(model.isEmpty())QSKIP("Model required");
    DetectionWorker worker;
    for(const auto &mode:QStringList{"cpu","directml","cpu","auto","cpu"}){
        const auto error=worker.configure(true,model,.5,mode);QVERIFY2(error.isEmpty(),qPrintable(error));
        QVERIFY(worker.backend().startsWith(mode=="cpu"?"CPU":"GPU"));
        QVERIFY(worker.detect(cv::Mat::zeros(100,100,CV_8UC3)).error.isEmpty());
    }
    QVERIFY(!worker.configure(true,model,.5,"invalid").isEmpty());
    QCOMPARE(worker.backend(),QString("CPU / OpenCV"));
    QVERIFY(worker.detect(cv::Mat::zeros(100,100,CV_8UC3)).error.isEmpty());
    QTemporaryDir missing;const auto old=qgetenv("SMART_MONITOR_GPU_DIR");
    const auto restore=qScopeGuard([&]{old.isNull()?qunsetenv("SMART_MONITOR_GPU_DIR"):qputenv("SMART_MONITOR_GPU_DIR",old);});
    qputenv("SMART_MONITOR_GPU_DIR",missing.path().toUtf8());
    QVERIFY(worker.configure(true,model,.5,"auto").isEmpty());
    QVERIFY(worker.backend().startsWith("CPU"));QVERIFY(worker.backend().contains("降级原因"));
    QVERIFY(worker.configure(true,model,.5,"cpu").isEmpty());
    QCOMPARE(worker.backend(),QString("CPU / OpenCV"));
 }
 void defaultsToCpu(){
    const auto model=qEnvironmentVariable("SMART_MONITOR_TEST_MODEL");if(model.isEmpty())QSKIP("Model required");
    const auto old=qgetenv("SMART_MONITOR_DETECTION_BACKEND");
    const auto restore=qScopeGuard([&]{old.isNull()?qunsetenv("SMART_MONITOR_DETECTION_BACKEND"):qputenv("SMART_MONITOR_DETECTION_BACKEND",old);});
    qunsetenv("SMART_MONITOR_DETECTION_BACKEND");
    DetectionWorker worker;QVERIFY(worker.configure(true,model,.5).isEmpty());
    QCOMPARE(worker.backend(),QString("CPU / OpenCV"));
 }
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
 void realModelRemovesDuplicateCandidates(){
    const auto model=qEnvironmentVariable("SMART_MONITOR_TEST_MODEL"),video=qEnvironmentVariable("SMART_MONITOR_TEST_VIDEO");
    if(model.isEmpty()||video.isEmpty())QSKIP("Real model/video required");
    cv::VideoCapture cap(video.toStdString(),cv::CAP_FFMPEG,{cv::CAP_PROP_N_THREADS,2});
    auto net=cv::dnn::readNetFromONNX(model.toStdString());
    net.setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);net.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);
    int removed=0;
    for(int sample=0;sample<3;++sample){
        cap.set(cv::CAP_PROP_POS_MSEC,sample*500);cv::Mat frame;QVERIFY(cap.read(frame));
        net.setInput(cv::dnn::blobFromImage(frame,1./255,cv::Size(640,640),cv::Scalar(),true,false));
        cv::Mat output=net.forward().clone();
        QVERIFY(output.dims==3 && output.size[1]==84);
        cv::Mat rows=output.reshape(1,output.size[1]);cv::transpose(rows,rows);
        int candidates=0;for(int i=0;i<rows.rows;++i)if(rows.at<float>(i,4)>=.5f)++candidates;
        const auto result=DetectionWorker::postprocess(output,frame.size(),.5);
        QVERIFY2(result.error.isEmpty(),qPrintable(result.error));QVERIFY(result.found);
        QVERIFY(int(result.boxes.size())<=candidates);
        for(size_t i=0;i<result.boxes.size();++i)for(size_t j=i+1;j<result.boxes.size();++j){
            const double intersection=(result.boxes[i]&result.boxes[j]).area();
            const double overlap=intersection/(result.boxes[i].area()+result.boxes[j].area()-intersection);
            QVERIFY2(overlap<=.450001,"Duplicate boxes survived NMS");
        }
        removed+=candidates-int(result.boxes.size());
        qInfo()<<"NMS sample ms"<<sample*500<<"raw person candidates"<<candidates<<"kept"<<result.boxes.size();
    }
    QVERIFY2(removed>0,"The real fixture must reproduce duplicate raw detections");
 }
 void initTestCase(){cv::setNumThreads(4);}
};
QTEST_GUILESS_MAIN(DetectionBackendTests)
#include "tst_detection_backend.moc"
