#include <QtTest>
#include <QSqlQuery>
#include "../service/operationlogservice.h"
class AuditWorkflowTests:public QObject{
 Q_OBJECT
private slots:
 void selectedDeleteAndEventNavigationStayScoped(){
    auto db=QSqlDatabase::addDatabase("QSQLITE","audit-flow");db.setDatabaseName(":memory:");QVERIFY(db.open());
    QSqlQuery q(db);
    QVERIFY(q.exec("CREATE TABLE operation_log(id INTEGER PRIMARY KEY,admin_id INTEGER,operation_time TEXT,operation_func TEXT,operation_desc TEXT)"));
    QVERIFY(q.exec("INSERT INTO operation_log VALUES(1,1,'2026-09-23','play','a'),(2,2,'2026-09-23','delete','b'),(3,NULL,'2026-09-23','warning','c')"));
    OperationLogService service(db);QString error;
    QVERIFY(!service.deleteIds({1,2},false,error));
    QVERIFY(service.deleteIds({1,2},true,error));
    QVERIFY(q.exec("SELECT id FROM operation_log"));QVERIFY(q.next());QCOMPARE(q.value(0).toInt(),3);QVERIFY(!q.next());
    QVERIFY(q.exec("CREATE TABLE exception_log(id INTEGER PRIMARY KEY,channel_id INTEGER,event_time TEXT,event_status TEXT,related_video_path TEXT)"));
    QVERIFY(q.exec("CREATE TABLE event_media_link(event_key TEXT,exception_id INTEGER,video_id INTEGER)"));
    QVERIFY(q.exec("CREATE TABLE video(id INTEGER PRIMARY KEY,video_path TEXT)"));
    QVERIFY(q.exec("INSERT INTO exception_log VALUES(7,2,'2026-09-23T10:00:00','complete','event.mp4'),(8,3,'2026-09-23T10:00:00','interrupted','other.mp4')"));
    QVERIFY(q.exec("INSERT INTO event_media_link VALUES('e',7,23)"));
    QList<EventLogEntry> entries;
    QVERIFY2(service.queryEvents(2,QDateTime(),QDateTime(),1,50,entries,error),qPrintable(error));
    QCOMPARE(entries.size(),1);QCOMPARE(entries.first().exceptionId,qint64(7));QCOMPARE(entries.first().videoId,qint64(23));
 }
};
QTEST_GUILESS_MAIN(AuditWorkflowTests)
#include "tst_audit_workflow.moc"
