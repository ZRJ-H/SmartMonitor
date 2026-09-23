#include <QtTest>
#include <QSqlQuery>
#include <QTemporaryDir>
#include "../service/eventrecordingstate.h"
#include "../service/eventservice.h"
class EventRecordingTests:public QObject {
 Q_OBJECT
private slots:
 void stateCapturesThreeFramesAndDoesNotRestartActiveEvent(){
    EventRecordingState state;
    QVERIFY(state.begin("event-a")); QVERIFY(!state.begin("event-b"));
    QVERIFY(state.takeSample(0)); QVERIFY(!state.takeSample(500));
    QVERIFY(state.takeSample(1000)); QVERIFY(state.takeSample(2000));
    QVERIFY(!state.takeSample(3000)); QVERIFY(!state.shouldFinish(59999));
    QVERIFY(state.shouldFinish(60000));
    QCOMPARE(state.key(),QString("event-a"));
    state.reset(); QVERIFY(!state.active()); QVERIFY(state.begin("event-b"));
 }
 void savesLinkedMediaExactlyOnce(){
    auto db=QSqlDatabase::addDatabase("QSQLITE","event-tests");db.setDatabaseName(":memory:");QVERIFY(db.open());
    QSqlQuery q(db);
    QVERIFY(q.exec("CREATE TABLE video(id INTEGER PRIMARY KEY AUTOINCREMENT,video_name TEXT,video_path TEXT UNIQUE,channel_id INTEGER,start_time TEXT,end_time TEXT,file_size INTEGER,video_type TEXT,exception_id INTEGER)"));
    QVERIFY(q.exec("CREATE TABLE exception_log(id INTEGER PRIMARY KEY AUTOINCREMENT,channel_id INTEGER,event_type TEXT,event_time TEXT,event_end_time TEXT,confidence REAL,event_status TEXT,event_desc TEXT,related_video_path TEXT)"));
    QVERIFY(q.exec("CREATE TABLE monitor_event(id INTEGER PRIMARY KEY AUTOINCREMENT,event_key TEXT UNIQUE,channel_id INTEGER,event_time TEXT,video_path TEXT,confidence REAL)"));
    QVERIFY(q.exec("CREATE TABLE event_media_link(event_key TEXT PRIMARY KEY,exception_id INTEGER,video_id INTEGER)"));
    QVERIFY(q.exec("CREATE TABLE feature_image(id INTEGER PRIMARY KEY AUTOINCREMENT,image_name TEXT,image_path TEXT UNIQUE,channel_id INTEGER,capture_time TEXT,capture_source TEXT,exception_id INTEGER,video_id INTEGER,video_offset_ms INTEGER)"));
    QTemporaryDir dir;
    QImage frame(16,16,QImage::Format_RGB32);frame.fill(Qt::green);
    auto time=QDateTime::currentDateTime();
    Video video("event.mp4",dir.filePath("event.mp4"),2,time,time.addSecs(60),100);
    EventService service(db,dir.path());QString error;
    QVERIFY2(service.save("key-a",video,.9,{frame,frame,frame},{0,1000,2000},true,error),qPrintable(error));
    QVERIFY2(service.save("key-a",video,.9,{frame,frame,frame},{0,1000,2000},true,error),qPrintable(error));
    QVERIFY(q.exec("SELECT COUNT(*),MIN(video_id),MIN(exception_id),MAX(video_offset_ms) FROM feature_image"));QVERIFY(q.next());
    QCOMPARE(q.value(0).toInt(),3);QVERIFY(q.value(1).toInt()>0);QVERIFY(q.value(2).toInt()>0);QCOMPARE(q.value(3).toInt(),2000);
    QVERIFY(q.exec("SELECT COUNT(*) FROM video"));QVERIFY(q.next());QCOMPARE(q.value(0).toInt(),1);
    QVERIFY(q.exec("SELECT event_status FROM exception_log"));QVERIFY(q.next());QCOMPARE(q.value(0).toString(),QString("complete"));
    // 缺帧的完整事件不允许形成“已完成”记录。
    QVERIFY(!service.save("key-b",video,.9,{frame},{0},true,error));
 }
};
QTEST_GUILESS_MAIN(EventRecordingTests)
#include "tst_event_recording.moc"
