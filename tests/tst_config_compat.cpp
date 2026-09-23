#include <QtTest>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QJsonDocument>
#include <QJsonObject>
#include "../service/systemconfigservice.h"

class ConfigCompatTests : public QObject {
    Q_OBJECT
private slots:
    void configurableCameraSources() {
        SystemConfig config; QString error;
        config.channelSources[0] = "camera:2";
        QVERIFY2(SystemConfigService::validate(config,error),qPrintable(error));
        config.channelSources[1] = "camera:bad";
        QVERIFY(!SystemConfigService::validate(config,error));
        config.channelSources[1] = "camera:2";
        QVERIFY(!SystemConfigService::validate(config,error));
    }
    void canonicalKeysWinAndSaveSynchronizesBothFormats() {
        auto db = QSqlDatabase::addDatabase("QSQLITE", "compat");
        db.setDatabaseName(":memory:");
        QVERIFY(db.open());
        QSqlQuery q(db);
        QVERIFY(q.exec("CREATE TABLE sys_config(config_key TEXT PRIMARY KEY,config_value TEXT)"));
        QVERIFY(q.exec("INSERT INTO sys_config VALUES('record.directory','C:/live'),('video_dir','C:/old'),('record.segment_seconds','10')"));
        const QString json = R"({"channelId":2,"name":"Parking","sourceType":"file","cameraIndex":-1,"filePath":"C:/parking.mp4"})";
        q.prepare("INSERT INTO sys_config VALUES('channel.2',:value)");
        q.bindValue(":value", json); QVERIFY(q.exec());
        SystemConfigService service(db);
        SystemConfig config; QString error;
        QVERIFY2(service.load(config, error), qPrintable(error));
        QCOMPARE(config.videoDirectory, QString("C:/live"));
        QCOMPARE(config.segmentSeconds, 10);
        QCOMPARE(config.channelNames.at(1), QString("Parking"));
        QCOMPARE(config.channelSources.at(1), QString("C:/parking.mp4"));
        QTemporaryDir directory;
        config.videoDirectory = directory.filePath("records");
        config.imageDirectory = directory.filePath("images");
        config.channelNames[1] = "New parking";
        QVERIFY2(service.save(config,error),qPrintable(error));
        QVERIFY(q.exec("SELECT config_value FROM sys_config WHERE config_key='record.directory'"));
        QVERIFY(q.next()); QCOMPARE(q.value(0).toString(), config.videoDirectory);
        QVERIFY(q.exec("SELECT config_value FROM sys_config WHERE config_key='channel.2'"));
        QVERIFY(q.next());
        QCOMPARE(QJsonDocument::fromJson(q.value(0).toByteArray()).object().value("name").toString(), QString("New parking"));
        // 后续字段写入失败时，不允许只更新一半配置。
        QVERIFY(q.exec("CREATE TRIGGER reject_interval BEFORE UPDATE ON sys_config WHEN NEW.config_key='capture_interval_ms' BEGIN SELECT RAISE(ABORT,'injected failure'); END"));
        config.videoDirectory = directory.filePath("different");
        QVERIFY(!service.save(config,error));
        QVERIFY(q.exec("SELECT config_value FROM sys_config WHERE config_key='record.directory'"));
        QVERIFY(q.next()); QCOMPARE(q.value(0).toString(),directory.filePath("records"));
    }
};
QTEST_GUILESS_MAIN(ConfigCompatTests)
#include "tst_config_compat.moc"
