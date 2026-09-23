#include <QtTest>
#include <QSqlQuery>
#include <QDir>
#include <QUuid>
#include <QSignalSpy>
#include <opencv2/videoio.hpp>
#include "../util/dbconn.h"
#include "../service/eventservice.h"
#include "../service/backupservice.h"
class MySQLIntegration:public QObject {
 Q_OBJECT
private slots:
 void actualForeignKeysMediaAndBackup(){
    QVERIFY2(qEnvironmentVariable("SMART_MONITOR_DB_NAME").startsWith("smart_monitor_verify_"),
             "Explicit isolated database required");
    QCoreApplication::addLibraryPath(qEnvironmentVariable("SMART_MONITOR_MYSQL_RUNTIME"));
    auto db=DbConn::getInstance()->database();
    QVERIFY2(db.isOpen(),qPrintable(DbConn::getInstance()->lastErrorText()));
    QCOMPARE(db.driverName(),QString("QMYSQL"));
    QSqlQuery q(db);QVERIFY(q.exec("SELECT COUNT(*) FROM video"));QVERIFY(q.next());int before=q.value(0).toInt();
    const QString key="integration-"+QUuid::createUuid().toString(QUuid::Id128);
    const QString folder=QDir(QCoreApplication::applicationDirPath()).filePath(key);QVERIFY(QDir().mkpath(folder));
    const QString path=QDir(folder).filePath("event.mp4");
    cv::VideoWriter writer(path.toStdString(),cv::VideoWriter::fourcc('m','p','4','v'),10,cv::Size(32,24));
    QVERIFY(writer.isOpened());
    for(int i=0;i<30;++i)writer.write(cv::Mat(24,32,CV_8UC3,cv::Scalar(0,255,0)));
    writer.release();cv::VideoCapture capture(path.toStdString());QVERIFY(capture.isOpened());
    QCOMPARE(qRound(capture.get(cv::CAP_PROP_FRAME_COUNT)/capture.get(cv::CAP_PROP_FPS)),3);
    const auto now=QDateTime::currentDateTime();
    Video video("event.mp4",path,1,now,now.addSecs(3),QFileInfo(path).size());
    QImage frame(32,24,QImage::Format_RGB32);frame.fill(Qt::green);
    EventService service(db,folder);QString error;
    QVERIFY2(service.save(key,video,.9,{frame,frame,frame},{0,1000,2000},true,error),qPrintable(error));
    QVERIFY2(service.save(key,video,.9,{frame,frame,frame},{0,1000,2000},true,error),qPrintable(error));
    QVERIFY(q.exec("SELECT COUNT(*) FROM video"));QVERIFY(q.next());QCOMPARE(q.value(0).toInt(),before+1);
    q.prepare("SELECT i.image_path,i.video_offset_ms FROM feature_image i JOIN event_media_link l ON l.exception_id=i.exception_id WHERE l.event_key=:key ORDER BY i.id");
    q.bindValue(":key",key);QVERIFY(q.exec());int count=0;
    while(q.next()){QVERIFY(!QImage(q.value(0).toString()).isNull());QCOMPARE(q.value(1).toInt(),count*1000);++count;}
    QCOMPARE(count,3);
    BackupService backup;QSignalSpy result(&backup,&BackupService::finished);
    QVERIFY(backup.start(db,folder,"C:/Program Files/MySQL/MySQL Server 5.7/bin/mysqldump.exe"));
    QTRY_COMPARE_WITH_TIMEOUT(result.size(),1,30000);
    QVERIFY2(result.first().first().toBool(),qPrintable(result.first().at(1).toString()));
    QVERIFY(QFileInfo(result.first().at(1).toString()).size()>1000);
 }
 void cleanupTestCase(){DbConn::releaseDbConn();}
};
QTEST_GUILESS_MAIN(MySQLIntegration)
#include "tst_mysql_integration.moc"
