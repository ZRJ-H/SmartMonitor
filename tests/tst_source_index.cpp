#include <QtTest>
#include <QScopeGuard>
#include <QTemporaryDir>
#include <QThread>
#include <QSqlQuery>
#include <QSqlError>
#include <QCryptographicHash>
#include <QUuid>
#include <QFile>
#include <opencv2/videoio.hpp>
#include "../service/sourceanalysisworker.h"
#include "../service/detectionworker.h"
namespace {
QByteArray fileHash(const QString &path){QFile f(path);if(!f.open(QIODevice::ReadOnly))return {};QCryptographicHash h(QCryptographicHash::Sha256);h.addData(&f);return h.result();}
struct Harness {
 QThread thread;SourceAnalysisWorker *worker;
 Harness(const QString &dir):worker(new SourceAnalysisWorker(dir)){worker->moveToThread(&thread);QObject::connect(&thread,&QThread::finished,worker,&QObject::deleteLater);thread.start();}
 ~Harness(){worker->cancel();thread.quit();thread.wait();}
 void start(int ch,const QString &path,const QString &model,double threshold=.5){QMetaObject::invokeMethod(worker,[=,w=worker]{w->configure(ch,path,true,model,threshold);},Qt::QueuedConnection);}
};
qint64 number(const QString &directory,const QString &sql){
 const auto name=QUuid::createUuid().toString();qint64 result=-1;
 {auto db=QSqlDatabase::addDatabase("QSQLITE",name);db.setDatabaseName(directory+"/source-index.sqlite");if(db.open()){QSqlQuery q(db);if(q.exec(sql)&&q.next())result=q.value(0).toLongLong();}}
 QSqlDatabase::removeDatabase(name);return result;
}
}
class SourceIndexTests:public QObject{
 Q_OBJECT
private:
 QString model,video;
 cv::Mat person;
 void createClip(const QString &path,bool withPeople=true){
   cv::VideoWriter out(path.toStdString(),cv::VideoWriter::fourcc('M','J','P','G'),10,person.size());QVERIFY(out.isOpened());
   for(int i=0;i<70;++i)out.write(withPeople&&(i<15||i>=55)?person:cv::Mat::zeros(person.size(),person.type()));
 }
private slots:
 void hotSwitchKeepsAnalysisTimeline(){
   QTemporaryDir dir;const QString source=dir.filePath("switch.avi"),index=dir.filePath("index");createClip(source);
   Harness h(index);QSignalSpy done(h.worker,&SourceAnalysisWorker::analysisFinished);
   QSignalSpy messages(h.worker,&SourceAnalysisWorker::message);
   int cycles=0;
   connect(h.worker,&SourceAnalysisWorker::inferenceCompleted,this,[&](int,qint64){
     ++cycles;
     if(cycles==1 || cycles==4 || cycles==7 || cycles==10){
       const bool gpu=cycles==1 || cycles==7;
       QMetaObject::invokeMethod(h.worker,[w=h.worker,gpu]{w->setGpuEnabled(gpu);},Qt::QueuedConnection);
     }
   });
   h.start(1,source,model);
   QTRY_COMPARE_WITH_TIMEOUT(done.count(),1,30000);
   QCOMPARE(done.first()[1].toInt(),2);
   QCOMPARE(number(index,"SELECT COUNT(*) FROM analyses"),1);
   QCOMPARE(number(index,"SELECT COUNT(*) FROM samples"),14);
   QCOMPARE(number(index,"SELECT COUNT(*) FROM events"),2);
   bool gpu=false,cpuAfterGpu=false;
   for(const auto &row:messages){const auto message=row[1].toString();
     QVERIFY2(!message.contains("失败"),qPrintable(message));
     if(message.contains("GPU / DirectML"))gpu=true;
     if(gpu&&message.contains("CPU / OpenCV"))cpuAfterGpu=true;
   }
   QVERIFY(gpu);QVERIFY(cpuAfterGpu);
 }
 void initTestCase(){
   cv::setNumThreads(4);model=qEnvironmentVariable("SMART_MONITOR_TEST_MODEL");video=qEnvironmentVariable("SMART_MONITOR_TEST_VIDEO");
   if(model.isEmpty()||video.isEmpty())QSKIP("Real YOLO/media inputs required for source-index integration");
   cv::VideoCapture cap(video.toStdString(),cv::CAP_FFMPEG,{cv::CAP_PROP_N_THREADS,2});QVERIFY(cap.read(person));
   DetectionWorker detector;QString error=detector.configure(true,model,.5);QVERIFY2(error.isEmpty(),qPrintable(error));QVERIFY(detector.detect(person).found);
 }
 void deduplicatesChannelsRestartRelocationAndDetectsReplacement(){
   QTemporaryDir dir;const QString source=dir.filePath("people.avi"),index=dir.filePath("index");createClip(source);const auto original=fileHash(source);
   {
     Harness h(index);QSignalSpy done(h.worker,&SourceAnalysisWorker::analysisFinished);QSignalSpy cycles(h.worker,&SourceAnalysisWorker::inferenceCompleted);
     for(int i=1;i<=4;++i)h.start(i,source,model);
     QTRY_COMPARE_WITH_TIMEOUT(done.count(),1,30000);
     QCOMPARE(done.first()[1].toInt(),2);
     QCOMPARE(number(index,"SELECT COUNT(*) FROM analyses"),1);
     QCOMPARE(number(index,"SELECT COUNT(*) FROM events"),2);
     QCOMPARE(number(index,"SELECT COUNT(*) FROM channel_analysis"),4);
     QCOMPARE(number(index,"SELECT COUNT(*) FROM samples"),14);
     const auto count=cycles.count();
     for(int repeat=0;repeat<20;++repeat)for(int i=1;i<=4;++i)h.worker->requestPosition(i,(repeat%7)*1000,1);
     QTest::qWait(250);QCOMPARE(cycles.count(),count);
   }
   QCOMPARE(QDir(index+"/screenshots").entryList({"*.jpg"},QDir::Files).size(),2);
   QCOMPARE(fileHash(source),original);
   const QString moved=dir.filePath("moved.avi");QVERIFY(QFile::rename(source,moved));
   {
     Harness h(index);QSignalSpy listed(h.worker,&SourceAnalysisWorker::eventsReady);QSignalSpy cycles(h.worker,&SourceAnalysisWorker::inferenceCompleted);
     h.start(1,moved,model);QTRY_VERIFY_WITH_TIMEOUT(listed.count()>0,10000);QTest::qWait(200);QCOMPARE(cycles.count(),0);
     QCOMPARE(number(index,"SELECT COUNT(*) FROM events"),2);
     const auto rows=qvariant_cast<QList<SourceEventRow>>(listed.last()[0]);QCOMPARE(rows.size(),2);QCOMPARE(rows.first().path,moved);
     QSignalSpy resolved(h.worker,&SourceAnalysisWorker::playbackResolved);
     QMetaObject::invokeMethod(h.worker,[&]{h.worker->resolvePlayback(rows.first(),1);},Qt::QueuedConnection);
     QTRY_COMPARE_WITH_TIMEOUT(resolved.count(),1,5000);QVERIFY(resolved.first()[2].toString().isEmpty());
     createClip(moved,false);QVERIFY(fileHash(moved)!=original);
     QMetaObject::invokeMethod(h.worker,[&]{h.worker->resolvePlayback(rows.first(),2);},Qt::QueuedConnection);
     QTRY_COMPARE_WITH_TIMEOUT(resolved.count(),2,5000);QVERIFY(!resolved.last()[2].toString().isEmpty());
     QSignalSpy done(h.worker,&SourceAnalysisWorker::analysisFinished);h.start(1,moved,model);
     QTRY_COMPARE_WITH_TIMEOUT(done.count(),1,30000);QCOMPARE(done.first()[1].toInt(),0);
     QCOMPARE(number(index,"SELECT COUNT(*) FROM analyses"),2);
   }
 }
 void resumesCheckpointWithoutDuplicateImages(){
   QTemporaryDir dir;const QString source=dir.filePath("resume.avi"),index=dir.filePath("index");createClip(source);
   {
     Harness h(index);QSignalSpy cycles(h.worker,&SourceAnalysisWorker::inferenceCompleted);h.start(1,source,model);
     QTRY_VERIFY_WITH_TIMEOUT(cycles.count()>=3,15000);
   }
   const qint64 before=number(index,"SELECT COUNT(*) FROM samples");QVERIFY(before>0);
   {
     Harness h(index);QSignalSpy done(h.worker,&SourceAnalysisWorker::analysisFinished);h.start(1,source,model);
     if(number(index,"SELECT COUNT(*) FROM analyses WHERE status='complete'")==0)QTRY_COMPARE_WITH_TIMEOUT(done.count(),1,30000);
     QTRY_COMPARE_WITH_TIMEOUT(number(index,"SELECT COUNT(*) FROM analyses WHERE status='complete'"),1,30000);
     QCOMPARE(number(index,"SELECT COUNT(*) FROM samples"),14);QCOMPARE(number(index,"SELECT COUNT(*) FROM events"),2);
   }
   QCOMPARE(QDir(index+"/screenshots").entryList({"*.jpg"},QDir::Files).size(),2);
 }
 void legacyUnfilteredIndexIsPreservedButNotReused(){
   QTemporaryDir dir;const QString source=dir.filePath("legacy.avi"),index=dir.filePath("index");createClip(source);
   // 用失败请求只建立临时索引结构，不接触应用的真实索引。
   {Harness h(index);QSignalSpy errors(h.worker,&SourceAnalysisWorker::message);
    h.start(1,dir.filePath("missing.avi"),model);QTRY_VERIFY_WITH_TIMEOUT(errors.count()>0,5000);}
   const QString asset=QString::fromLatin1(fileHash(source).toHex());
   const QString profile=QString::fromLatin1(fileHash(model).toHex())+"|stretch640-person-v1|0.5|sample500-gap3000-one-image-v1";
   const QString legacyKey=QString::fromLatin1(QCryptographicHash::hash((asset+"|"+profile).toUtf8(),QCryptographicHash::Sha256).toHex());
   const QString name=QUuid::createUuid().toString();
   {const auto cleanup=qScopeGuard([&]{QSqlDatabase::removeDatabase(name);});
    auto db=QSqlDatabase::addDatabase("QSQLITE",name);db.setDatabaseName(index+"/source-index.sqlite");QVERIFY(db.open());
    auto execute=[&](const QString &sql,const QVariantList &args){QSqlQuery q(db);if(!q.prepare(sql))return false;for(auto a:args)q.addBindValue(a);return q.exec();};
    const QFileInfo file(source);
    QVERIFY(execute("INSERT INTO assets(asset,path,size,modified) VALUES(?,?,?,?)",{asset,source,file.size(),file.lastModified().toMSecsSinceEpoch()}));
    QVERIFY(execute("INSERT INTO analyses(analysis,asset,profile,status,analyzed_at) VALUES(?,?,?,'complete','legacy')",{legacyKey,asset,profile}));
    QVERIFY(execute("INSERT INTO samples(analysis,sample,pts,boxes) VALUES(?,0,0,'[[1,1,20,30],[1,1,20,30]]')",{legacyKey}));
    QVERIFY(execute("INSERT INTO channel_analysis(channel,analysis) VALUES(1,?)",{legacyKey}));
   }
   {Harness h(index);QSignalSpy done(h.worker,&SourceAnalysisWorker::analysisFinished);QSignalSpy cycles(h.worker,&SourceAnalysisWorker::inferenceCompleted);
    h.start(1,source,model);QTRY_COMPARE_WITH_TIMEOUT(done.count(),1,30000);QVERIFY(cycles.count()>0);
    QCOMPARE(number(index,"SELECT COUNT(*) FROM analyses"),2);
    QCOMPARE(number(index,"SELECT COUNT(*) FROM samples WHERE analysis='"+legacyKey+"'"),1);
    QCOMPARE(number(index,"SELECT COUNT(*) FROM channel_analysis WHERE analysis='"+legacyKey+"'"),0);
    QCOMPARE(number(index,"SELECT COUNT(*) FROM samples WHERE analysis=(SELECT analysis FROM channel_analysis WHERE channel=1)"),14);
   }
   // 新版结果可在重启后正常复用，不会每次启动重新分析。
   {Harness h(index);QSignalSpy listed(h.worker,&SourceAnalysisWorker::eventsReady);QSignalSpy cycles(h.worker,&SourceAnalysisWorker::inferenceCompleted);
    h.start(1,source,model);QTRY_VERIFY_WITH_TIMEOUT(listed.count()>0,10000);QTest::qWait(200);QCOMPARE(cycles.count(),0);}
 }
 void invalidModelCannotCompleteAnalysis(){
   QTemporaryDir dir;createClip(dir.filePath("bad.avi"));Harness h(dir.filePath("index"));QSignalSpy errors(h.worker,&SourceAnalysisWorker::message);
   h.start(1,dir.filePath("bad.avi"),dir.filePath("missing.onnx"));QTRY_VERIFY_WITH_TIMEOUT(errors.count()>0,5000);
   QCOMPARE(number(dir.filePath("index"),"SELECT COUNT(*) FROM analyses WHERE status='complete'"),0);
 }
};
QTEST_GUILESS_MAIN(SourceIndexTests)
#include "tst_source_index.moc"
