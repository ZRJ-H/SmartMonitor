#include <QtTest>
#include <QSqlQuery>
#include "../service/videoservice.h"
class VideoServiceTests : public QObject {
    Q_OBJECT
private slots:
    void lookupAndEventFilteringPreserveOutputsOnFailure() {
        auto db=QSqlDatabase::addDatabase("QSQLITE","video-query");
        db.setDatabaseName(":memory:"); QVERIFY(db.open());
        QSqlQuery q(db);
        QVERIFY(q.exec("CREATE TABLE video(id INTEGER PRIMARY KEY,video_name TEXT,video_path TEXT,channel_id INTEGER,start_time TEXT,end_time TEXT,file_size INTEGER)"));
        QVERIFY(q.exec("CREATE TABLE monitor_event(id INTEGER PRIMARY KEY,event_key TEXT UNIQUE,channel_id INTEGER,event_time TEXT,video_path TEXT,confidence REAL)"));
        QVERIFY(q.exec("INSERT INTO video VALUES(1,'normal','normal.mp4',1,'2026-09-23T10:00:00','2026-09-23T10:01:00',10),(2,'event','event.mp4',2,'2026-09-23T10:00:00','2026-09-23T10:01:00',20)"));
        QVERIFY(q.exec("INSERT INTO monitor_event VALUES(1,'e',2,'2026-09-23T10:00:00','event.mp4',0.9)"));
        VideoService service(db,1); QString error;
        VideoRecord record;
        QVERIFY(service.findById(2,record,error));
        QCOMPARE(record.channelId,2); QCOMPARE(record.path,QString("event.mp4"));
        QVERIFY(!service.findById(999,record,error));
        QCOMPARE(record.id,2);
        VideoQuery filter; filter.eventType=1;
        VideoPage page; QVERIFY(service.loadPage(filter,1,page,error));
        QCOMPARE(page.totalRecords,1); QCOMPARE(page.records.first().id,2);
        filter.eventType=0;
        QVERIFY(service.loadPage(filter,1,page,error));
        QCOMPARE(page.totalRecords,1); QCOMPARE(page.records.first().id,1);
        filter.startTime=QDateTime::currentDateTime(); filter.endTime=filter.startTime.addSecs(-1);
        QVERIFY(!service.loadPage(filter,1,page,error));
        QCOMPARE(page.records.first().id,1);
    }
};
QTEST_GUILESS_MAIN(VideoServiceTests)
#include "tst_video_service.moc"
