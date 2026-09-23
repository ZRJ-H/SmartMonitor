#include <QtTest>
#include <QTemporaryDir>
#include <QSqlQuery>
#include <QFile>
#include <QDir>
#include "../service/mediarecoveryservice.h"
class MediaRecoveryTests:public QObject {
 Q_OBJECT
private slots:
 void failedDatabaseWriteSurvivesRestartAndReplaysOnce(){
    QTemporaryDir dir;
    auto db=QSqlDatabase::addDatabase("QSQLITE","recovery");db.setDatabaseName(":memory:");QVERIFY(db.open());
    const auto now=QDateTime::currentDateTime();
    const QString path=dir.filePath("kept.mp4");QFile file(path);QVERIFY(file.open(QIODevice::WriteOnly));file.write("video");file.close();
    Video video("kept.mp4",path,1,now,now.addSecs(10),5);
    QString error;
    {
        MediaRecoveryService service(db,dir.filePath("pending"),dir.filePath("images"));
        QVERIFY(!service.submit(video,QString(),0,{}, {},true,error));
        QCOMPARE(QDir(dir.filePath("pending")).entryList({"*.json"},QDir::Files).size(),1);
    }
    QSqlQuery q(db);
    QVERIFY(q.exec("CREATE TABLE video(id INTEGER PRIMARY KEY,video_name TEXT,video_path TEXT UNIQUE,channel_id INTEGER,start_time TEXT,end_time TEXT,file_size INTEGER)"));
    QVERIFY(q.exec("CREATE TABLE monitor_event(video_path TEXT)"));
    MediaRecoveryService restarted(db,dir.filePath("pending"),dir.filePath("images"));
    QStringList errors;QCOMPARE(restarted.replay(errors),1);QVERIFY(errors.isEmpty());
    QCOMPARE(restarted.replay(errors),0);
    QVERIFY(q.exec("SELECT COUNT(*) FROM video"));QVERIFY(q.next());QCOMPARE(q.value(0).toInt(),1);
    QVERIFY(QFile::exists(path));
 }
};
QTEST_GUILESS_MAIN(MediaRecoveryTests)
#include "tst_media_recovery.moc"
