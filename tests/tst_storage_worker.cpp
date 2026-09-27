#include <QtTest>
#include <QTemporaryDir>
#include <QSqlQuery>
#include <QSqlError>
#include <QFile>
#include <QTimer>
#include <QScopeGuard>
#include <thread>
#include <vector>
#include "../service/storageworker.h"
#include "../service/mediarecoveryservice.h"
class StorageWorkerTests:public QObject{
 Q_OBJECT
private slots:
 void stagesBeforeQueueAndReplaysAfterDatabaseFailure(){
   QTemporaryDir dir;auto db=QSqlDatabase::addDatabase("QSQLITE","storage-test");db.setDatabaseName(dir.filePath("business.sqlite"));QVERIFY(db.open());auto close=qScopeGuard([&]{db.close();});
   const QString videoPath=dir.filePath("kept.mp4");QFile file(videoPath);QVERIFY(file.open(QIODevice::WriteOnly));file.write("existing sealed media");file.close();
   const auto now=QDateTime::currentDateTime();Video video("kept.mp4",videoPath,1,now,now.addSecs(1),20);
   {
     StorageWorker worker(StorageConnection::from(db));worker.setDirectories(dir.filePath("pending"),dir.filePath("images"));
     QSignalSpy warnings(&worker,&StorageWorker::warning);worker.start();
     worker.submit(video); // test producer; production calls only from recording threads
     QCOMPARE(QDir(dir.filePath("pending")).entryList({"*.json"}).size(),1);
     QTRY_VERIFY_WITH_TIMEOUT(warnings.count()>0,5000);worker.finish();QVERIFY(worker.wait(5000));
   }
   QCOMPARE(QDir(dir.filePath("pending")).entryList({"*.json"}).size(),1);
   QSqlQuery q(db);QVERIFY(q.exec("CREATE TABLE video(id INTEGER PRIMARY KEY,video_name TEXT,video_path TEXT UNIQUE,channel_id INTEGER,start_time TEXT,end_time TEXT,file_size INTEGER)"));QVERIFY(q.exec("CREATE TABLE monitor_event(video_path TEXT)"));
   {
     StorageWorker worker(StorageConnection::from(db));worker.setDirectories(dir.filePath("pending"),dir.filePath("images"));worker.start();worker.requestReplay();
     QTRY_COMPARE_WITH_TIMEOUT(QDir(dir.filePath("pending")).entryList({"*.json"}).size(),0,5000);worker.finish();QVERIFY(worker.wait(5000));
   }
   QVERIFY(q.exec("SELECT COUNT(*) FROM video"));QVERIFY(q.next());QCOMPARE(q.value(0).toInt(),1);QVERIFY(QFile::exists(videoPath));
 }
 void replayRacingProducersDoesNotConsumeQueuedManifests(){
   QTemporaryDir dir;auto db=QSqlDatabase::addDatabase("QSQLITE","concurrent-storage");db.setDatabaseName(dir.filePath("business.sqlite"));QVERIFY(db.open());auto close=qScopeGuard([&]{db.close();});
   QSqlQuery q(db);QVERIFY(q.exec("CREATE TABLE video(id INTEGER PRIMARY KEY,video_name TEXT,video_path TEXT UNIQUE,channel_id INTEGER,start_time TEXT,end_time TEXT,file_size INTEGER)"));QVERIFY(q.exec("CREATE TABLE monitor_event(video_path TEXT)"));
   QList<Video> videos;const auto now=QDateTime::currentDateTime();
   for(int i=0;i<40;++i){QString name=QString::number(i)+".mp4";QFile file(dir.filePath(name));QVERIFY(file.open(QIODevice::WriteOnly));file.write("sealed");file.close();videos<<Video(name,file.fileName(),i%4+1,now,now.addSecs(1),6);}
   StorageWorker worker(StorageConnection::from(db));worker.setDirectories(dir.filePath("pending"),dir.filePath("images"));QSignalSpy saved(&worker,&StorageWorker::saved),warnings(&worker,&StorageWorker::warning);worker.start();
   std::vector<std::thread> producers;
   for(int channel=0;channel<4;++channel)producers.emplace_back([&,channel]{for(int i=channel;i<40;i+=4){worker.requestReplay();worker.submit(videos.at(i));worker.requestReplay();}});
   for(auto &producer:producers)producer.join();
   QTRY_COMPARE_WITH_TIMEOUT(saved.count(),40,10000);worker.finish();QVERIFY(worker.wait(5000));QCOMPARE(warnings.count(),0);
   QCOMPARE(QDir(dir.filePath("pending")).entryList({"*.json"}).size(),0);
   QVERIFY(q.exec("SELECT COUNT(*) FROM video"));QVERIFY(q.next());QCOMPARE(q.value(0).toInt(),40);
 }
 void slowSqlDoesNotBlockEventLoop(){
   QTemporaryDir dir;auto db=QSqlDatabase::addDatabase("QSQLITE","slow-storage");db.setDatabaseName(dir.filePath("locked.sqlite"));QVERIFY(db.open());auto close=qScopeGuard([&]{db.close();});
   QSqlQuery q(db);QVERIFY(q.exec("CREATE TABLE video(id INTEGER PRIMARY KEY,video_name TEXT,video_path TEXT,channel_id INTEGER,start_time TEXT,end_time TEXT,file_size INTEGER)"));QVERIFY(q.exec("CREATE TABLE monitor_event(video_path TEXT)"));
   QFile file(dir.filePath("media.mp4"));QVERIFY(file.open(QIODevice::WriteOnly));file.write("sealed video");file.close();
   StorageWorker worker(StorageConnection::from(db));worker.setDirectories(dir.filePath("pending"),dir.filePath("images"));worker.start();
   QSignalSpy saved(&worker,&StorageWorker::saved);
   QVERIFY(q.exec("BEGIN EXCLUSIVE"));
   const auto now=QDateTime::currentDateTime();worker.submit(Video("media.mp4",file.fileName(),1,now,now.addSecs(1),12));
   int beats=0;QTimer timer;connect(&timer,&QTimer::timeout,[&]{++beats;});timer.start(20);QTest::qWait(500);QVERIFY(beats>=15);QCOMPARE(saved.count(),0);
   QVERIFY(q.exec("COMMIT"));QTRY_COMPARE_WITH_TIMEOUT(saved.count(),1,6000);worker.finish();QVERIFY(worker.wait(5000));
 }
};
QTEST_GUILESS_MAIN(StorageWorkerTests)
#include "tst_storage_worker.moc"
