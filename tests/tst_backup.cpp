#include <QtTest>
#include <QSignalSpy>
#include <QTemporaryDir>
#include "../service/backupservice.h"
class BackupTests:public QObject {
 Q_OBJECT
private slots:
 void missingToolDoesNotCreateSuccessfulBackup() {
    QTemporaryDir directory;
    auto db=QSqlDatabase::addDatabase("QSQLITE","backup"); db.setDatabaseName(":memory:"); QVERIFY(db.open());
    BackupService service; QSignalSpy result(&service,&BackupService::finished);
    QVERIFY(!service.start(db,directory.path(),"C:/missing/mysqldump.exe"));
    QCOMPARE(result.size(),1); QVERIFY(!result.first().first().toBool());
    QCOMPARE(QDir(directory.path()).entryList({"*.sql"},QDir::Files).size(),0);
 }
};
QTEST_GUILESS_MAIN(BackupTests)
#include "tst_backup.moc"
