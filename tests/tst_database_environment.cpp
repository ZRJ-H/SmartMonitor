#include <QtTest>
#include <QSqlQuery>
#include "../util/dbconn.h"
#include "../model/configmodel.h"
class DatabaseEnvironmentTests : public QObject {
    Q_OBJECT
private slots:
    void configuredDatabaseNeverFallsBackToProduction() {
        qputenv("SMART_MONITOR_DB_DRIVER","QSQLITE");
        qputenv("SMART_MONITOR_DB_NAME",":memory:");
        auto connection = DbConn::getInstance();
        QCOMPARE(connection->database().driverName(), QString("QSQLITE"));
        QVERIFY(connection->isOpen());
        QSqlQuery query(connection->database());
        QVERIFY(query.exec("CREATE TABLE sys_config(config_key TEXT PRIMARY KEY,config_value TEXT)"));
        ConfigModel model;
        QVERIFY(model.upsertValue("key","first"));
        QVERIFY(model.upsertValue("key","second"));
        QString value; bool found=false;
        QVERIFY(model.findValue("key",value,found));
        QVERIFY(found); QCOMPARE(value,QString("second"));
    }
    void cleanupTestCase() { DbConn::releaseDbConn(); }
};
QTEST_GUILESS_MAIN(DatabaseEnvironmentTests)
#include "tst_database_environment.moc"
