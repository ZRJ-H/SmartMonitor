#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include "../service/recordtask.h"
class RecordTaskEventTests:public QObject {
 Q_OBJECT
private slots:
 void switchesToEventWithoutOverwritingNormalSegment(){
    QTemporaryDir directory;RecordTask task;
    int normalCount=0,eventCount=0;QString normalPath,eventPath;
    connect(&task,&RecordTask::segmentSaved,this,[&](const Video &v){++normalCount;normalPath=v.getVideoPath();});
    connect(&task,&RecordTask::eventSegmentSaved,this,[&](const Video &v,const QString &key,double,const QList<QImage> &frames,const QList<qint64> &offsets,bool complete){
        ++eventCount;eventPath=v.getVideoPath();QCOMPARE(key,QString("test-event"));
        QCOMPARE(frames.size(),3);QCOMPARE(offsets.size(),3);QVERIFY(!complete);
    });
    task.configure(1,10,directory.path(),10);
    cv::Mat frame(24,32,CV_8UC3,cv::Scalar(0,255,0));
    task.saveFrame(frame,1);
    task.beginEvent("test-event",.9);
    for(int i=0;i<3;++i){QTest::qWait(i==0?120:1100);task.saveFrame(frame,1);}
    task.stop();
    QCOMPARE(normalCount,1);QCOMPARE(eventCount,1);QVERIFY(normalPath!=eventPath);
    cv::VideoCapture normal(normalPath.toStdString()),event(eventPath.toStdString());
    QVERIFY(normal.isOpened());QVERIFY(event.isOpened());
 }
 void sameSecondSegmentUsesAnotherFileName(){
    QTemporaryDir dir;RecordingSegment segment;
    auto time=QDateTime(QDate(2026,9,23),QTime(12,0));
    segment.begin(dir.path(),1,time);auto first=segment.filePath();
    QFile file(first);QVERIFY(file.open(QIODevice::WriteOnly));file.write("keep");file.close();
    segment.finish(time,4);segment.begin(dir.path(),1,time);QVERIFY(segment.filePath()!=first);
 }
};
QTEST_GUILESS_MAIN(RecordTaskEventTests)
#include "tst_recordtask_event.moc"
