#include <QtTest>
#include <limits>
#include "../service/detectionworker.h"

namespace {
cv::Mat predictions() { return cv::Mat::zeros(100, 84, CV_32F); }
void person(cv::Mat &rows, int row, float cx, float cy, float w, float h, float score) {
    float *p = rows.ptr<float>(row);
    p[0]=cx; p[1]=cy; p[2]=w; p[3]=h; p[4]=score;
}
DetectionResult decode(const cv::Mat &rows) {
    const int dims[] = {1, rows.rows, rows.cols};
    return DetectionWorker::postprocess(rows.reshape(1,3,dims), cv::Size(640,640), .5);
}
}
class DetectionPostprocessTests : public QObject {
    Q_OBJECT
private slots:
    void suppressesDuplicateButKeepsAnotherPerson() {
        auto rows=predictions();
        person(rows,0,100,100,80,160,.95f);
        person(rows,1,102,101,80,160,.85f);
        person(rows,2,400,100,80,160,.90f);
        const auto result=decode(rows);
        QVERIFY2(result.error.isEmpty(),qPrintable(result.error));
        QVERIFY(result.found);
        QCOMPARE(result.boxes.size(),size_t(2));
        QVERIFY(result.boxes[0]==cv::Rect(60,20,80,160));
        QVERIFY(qAbs(result.confidence-.95)<1e-6);
    }
    void clipsEndpointsWithoutExpandingBox() {
        auto rows=predictions();person(rows,0,0,100,40,60,.9f);
        const auto result=decode(rows);
        QCOMPARE(result.boxes.size(),size_t(1));
        QVERIFY(result.boxes[0]==cv::Rect(0,70,20,60));
    }
    void offscreenCandidateCannotTriggerEvent() {
        auto rows=predictions();person(rows,0,900,100,40,60,.9f);
        const auto result=decode(rows);
        QVERIFY(result.boxes.empty());
        QVERIFY(!result.found);
        QCOMPARE(result.confidence,0.0);
    }
    void supportsBothYoloLayoutsAndObjectness_data() {
        QTest::addColumn<int>("columns"); QTest::addColumn<bool>("transposed");
        QTest::newRow("v8-N84")<<84<<false;
        QTest::newRow("v8-84N")<<84<<true;
        QTest::newRow("v5-N85")<<85<<false;
        QTest::newRow("v5-85N")<<85<<true;
    }
    void supportsBothYoloLayoutsAndObjectness() {
        QFETCH(int,columns); QFETCH(bool,transposed);
        cv::Mat rows=cv::Mat::zeros(3,columns,CV_32F);
        person(rows,0,100,100,80,160,.9f);
        person(rows,1,101,100,80,160,.8f);
        person(rows,2,400,100,80,160,.4f);
        if(columns==85) {rows.at<float>(0,5)=.8f;rows.at<float>(1,5)=.9f;rows.at<float>(2,5)=1;}
        if(transposed) cv::transpose(rows,rows);
        auto result=decode(rows);
        QVERIFY2(result.error.isEmpty(),qPrintable(result.error));
        QCOMPARE(result.boxes.size(),size_t(1));
        QVERIFY(qAbs(result.confidence-(columns==84?.9:.72))<1e-6);
    }
    void keepsThresholdEqualityAndIgnoresOtherClasses() {
        auto rows=predictions();person(rows,0,100,100,40,60,.5f);
        person(rows,1,400,100,40,60,.49f);rows.at<float>(1,5)=.99f;
        auto result=decode(rows);QCOMPARE(result.boxes.size(),size_t(1));
        QCOMPARE(result.confidence,.5);
    }
    void scalesAndClipsAllEdges() {
        auto rows=predictions();person(rows,0,640,640,40,60,.9f);
        const int dims[]={1,rows.rows,rows.cols};
        auto result=DetectionWorker::postprocess(rows.reshape(1,3,dims),cv::Size(1280,320),.5);
        QCOMPARE(result.boxes.size(),size_t(1));
        QVERIFY(result.boxes[0]==cv::Rect(1240,305,40,15));
        person(rows,0,0,0,40,60,.9f);
        result=decode(rows);QVERIFY(result.boxes[0]==cv::Rect(0,0,20,30));
    }
    void rejectsInvalidCandidatesAndDoesNotTriggerEvent() {
        auto rows=predictions();
        for(int i=0;i<7;++i) person(rows,i,100,100,40,60,.9f);
        rows.at<float>(0,4)=std::numeric_limits<float>::quiet_NaN();
        rows.at<float>(1,0)=std::numeric_limits<float>::infinity();
        rows.at<float>(2,2)=-1;
        rows.at<float>(3,3)=0;
        rows.at<float>(4,4)=2;
        rows.at<float>(5,1)=-500;
        rows.at<float>(6,0)=std::numeric_limits<float>::max();
        const auto result=decode(rows);
        QVERIFY(result.error.isEmpty());QVERIFY(result.boxes.empty());
        QVERIFY(!result.found);QCOMPARE(result.confidence,0.0);
    }
    void unsupportedTensorIsAnErrorNotAnEmptySuccess() {
        auto check=[](const cv::Mat &tensor) {
            const auto result=DetectionWorker::postprocess(tensor,cv::Size(640,640),.5);
            return !result.error.isEmpty() && !result.found && result.boxes.empty();
        };
        QVERIFY(check(cv::Mat()));QVERIFY(check(predictions()));
        const int badLayout[]={1,100,6},batch[]={2,100,84},valid[]={1,100,84};
        QVERIFY(check(cv::Mat::zeros(3,badLayout,CV_32F)));
        QVERIFY(check(cv::Mat::zeros(3,batch,CV_32F)));
        QVERIFY(check(cv::Mat::zeros(3,valid,CV_64F)));
    }
};
QTEST_GUILESS_MAIN(DetectionPostprocessTests)
#include "tst_detection_postprocess.moc"