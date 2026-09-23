#include <QtTest>

#include "../service/imageservice.h"
#include <QSqlError>
#include "../service/operationlogservice.h"
#include "../service/systemconfigservice.h"

#include <QDir>
#include <QImage>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>

class Day7Day8Tests : public QObject
{
    Q_OBJECT

private slots:
    void systemWarningsHaveNoInventedAdministrator() {
        OperationLogService service(database);QString error;
        QVERIFY(service.record(0,"磁盘告警","剩余10%",QDateTime::currentDateTime(),error));
        QSqlQuery query(database);
        QVERIFY(query.exec("SELECT admin_id FROM operation_log"));
        QVERIFY(query.next());QVERIFY(query.value(0).isNull());
    }

    void init();
    void cleanup();
    void eventCaptureSavesExactlyThreeLinkedImages();
    void eventCaptureRejectsIncompleteFrameSetAtomically();
    void manualCapturePreservesVideoPosition();
    void imageQueryExportAndConfirmedDeleteWork();
    void settingsValidatePersistAndReload();
    void logsFilterNewestFirstAndRequireDeleteConfirmation();

private:
    QSqlDatabase database;
    QString connectionName;
};

void Day7Day8Tests::init()
{
    connectionName = QStringLiteral("day7_day8_%1")
            .arg(reinterpret_cast<quintptr>(this));
    database = QSqlDatabase::addDatabase("QSQLITE", connectionName);
    database.setDatabaseName(":memory:");
    QVERIFY2(database.open(), qPrintable(database.lastError().text()));

    QSqlQuery query(database);
    QVERIFY(query.exec("CREATE TABLE feature_image ("
                       "id INTEGER PRIMARY KEY AUTOINCREMENT, image_name TEXT NOT NULL, "
                       "image_path TEXT NOT NULL UNIQUE, channel_id INTEGER NOT NULL, "
                       "capture_time TEXT NOT NULL, capture_source TEXT NOT NULL, "
                       "exception_id INTEGER, video_id INTEGER, video_offset_ms INTEGER)"));
    QVERIFY(query.exec("CREATE TABLE sys_config ("
                       "config_key TEXT PRIMARY KEY, config_value TEXT)"));
    QVERIFY(query.exec("CREATE TABLE operation_log ("
                       "id INTEGER PRIMARY KEY AUTOINCREMENT, admin_id INTEGER, "
                       "operation_time TEXT NOT NULL, operation_func TEXT NOT NULL, "
                       "operation_desc TEXT)"));
}

void Day7Day8Tests::cleanup()
{
    database.close();
    database = QSqlDatabase();
    QSqlDatabase::removeDatabase(connectionName);
}

void Day7Day8Tests::eventCaptureSavesExactlyThreeLinkedImages()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ImageService service(database, directory.path());
    const QDateTime capturedAt(QDate(2026, 9, 22), QTime(10, 30, 45));
    const QList<QImage> frames = {
        QImage(8, 8, QImage::Format_RGB32),
        QImage(8, 8, QImage::Format_RGB32),
        QImage(8, 8, QImage::Format_RGB32)
    };

    QList<FeatureImage> saved;
    QString error;
    QVERIFY2(service.saveEventImages(frames, 2, 17, capturedAt, saved, error),
             qPrintable(error));
    QCOMPARE(saved.size(), 3);
    QCOMPARE(saved.at(0).imageName,
             QString::fromUtf8("异常_通道2_20260922103045_1.jpg"));
    QCOMPARE(saved.at(2).exceptionId, 17);
    QCOMPARE(saved.at(2).captureSource, QStringLiteral("event"));
    for (const FeatureImage &image : saved) {
        QVERIFY(QFileInfo::exists(image.imagePath));
    }

    QSqlQuery count(database);
    QVERIFY(count.exec("SELECT COUNT(*) FROM feature_image"));
    QVERIFY(count.next());
    QCOMPARE(count.value(0).toInt(), 3);
}

void Day7Day8Tests::eventCaptureRejectsIncompleteFrameSetAtomically()
{
    QTemporaryDir directory;
    ImageService service(database, directory.path());
    QList<FeatureImage> saved;
    QString error;

    QVERIFY(!service.saveEventImages({QImage(4, 4, QImage::Format_RGB32)},
                                     1, 99, QDateTime::currentDateTime(), saved, error));
    QVERIFY(error.contains(QString::fromUtf8("正好3张")));
    QCOMPARE(QDir(directory.path()).entryList(QDir::Files).size(), 0);

    QSqlQuery count(database);
    QVERIFY(count.exec("SELECT COUNT(*) FROM feature_image"));
    QVERIFY(count.next());
    QCOMPARE(count.value(0).toInt(), 0);
}

void Day7Day8Tests::manualCapturePreservesVideoPosition()
{
    QTemporaryDir directory;
    ImageService service(database, directory.path());
    FeatureImage saved;
    QString error;
    const QDateTime capturedAt(QDate(2026, 9, 22), QTime(11, 2, 3));

    QVERIFY2(service.saveManualImage(QImage(8, 8, QImage::Format_RGB32),
                                     4, capturedAt, 23, 4567, saved, error),
             qPrintable(error));
    QCOMPARE(saved.channelId, 4);
    QCOMPARE(saved.videoId, 23);
    QCOMPARE(saved.videoOffsetMs, 4567);
    QCOMPARE(saved.captureSource, QStringLiteral("manual"));
    QVERIFY(QFileInfo::exists(saved.imagePath));
}

void Day7Day8Tests::imageQueryExportAndConfirmedDeleteWork()
{
    QTemporaryDir imageDirectory;
    QTemporaryDir exportDirectory;
    ImageService service(database, imageDirectory.path());
    QString error;
    FeatureImage first;
    FeatureImage second;
    QVERIFY(service.saveManualImage(QImage(8, 8, QImage::Format_RGB32), 1,
                                    QDateTime(QDate(2026, 9, 21), QTime(9, 0)),
                                    0, 0, first, error));
    QVERIFY(service.saveManualImage(QImage(8, 8, QImage::Format_RGB32), 2,
                                    QDateTime(QDate(2026, 9, 22), QTime(9, 0)),
                                    0, 0, second, error));

    ImageFilter filter;
    filter.channelId = 2;
    QList<FeatureImage> results;
    QVERIFY2(service.query(filter, 1, 9, results, error), qPrintable(error));
    QCOMPARE(results.size(), 1);
    QCOMPARE(results.first().id, second.id);

    QVERIFY2(service.exportImages({first.id, second.id}, exportDirectory.path(), error),
             qPrintable(error));
    QCOMPARE(QDir(exportDirectory.path()).entryList(QDir::Files).size(), 2);

    QVERIFY(!service.deleteImages({first.id}, false, error));
    QVERIFY(error.contains(QString::fromUtf8("确认")));
    QVERIFY(QFileInfo::exists(first.imagePath));
    QVERIFY2(service.deleteImages({first.id}, true, error), qPrintable(error));
    QVERIFY(!QFileInfo::exists(first.imagePath));
}

void Day7Day8Tests::settingsValidatePersistAndReload()
{
    QTemporaryDir videoDirectory;
    QTemporaryDir imageDirectory;
    SystemConfigService service(database);
    SystemConfig config;
    config.videoDirectory = videoDirectory.path();
    config.imageDirectory = imageDirectory.path();
    config.segmentSeconds = 60;
    config.captureIntervalMs = 40;
    config.channelNames = {"East", "West", "Park", "Server"};
    config.channelSources = {"camera:0", "file:a.mp4", "file:b.mp4", "file:c.mp4"};
    QString error;

    QVERIFY2(service.save(config, error), qPrintable(error));
    SystemConfig loaded;
    QVERIFY2(service.load(loaded, error), qPrintable(error));
    QCOMPARE(loaded.videoDirectory, QDir::cleanPath(videoDirectory.path()));
    QCOMPARE(loaded.segmentSeconds, 60);
    QCOMPARE(loaded.channelNames.at(3), QString("Server"));

    config.segmentSeconds = 0;
    QVERIFY(!service.save(config, error));
    QVERIFY(error.contains(QString::fromUtf8("分段时长")));
}

void Day7Day8Tests::logsFilterNewestFirstAndRequireDeleteConfirmation()
{
    OperationLogService service(database);
    QString error;
    QVERIFY(service.record(1, "login", "ok",
                           QDateTime(QDate(2026, 9, 22), QTime(8, 0)), error));
    QVERIFY(service.record(1, "screenshot", "channel 2",
                           QDateTime(QDate(2026, 9, 22), QTime(9, 0)), error));
    QVERIFY(service.record(2, "login", "ok",
                           QDateTime(QDate(2026, 9, 22), QTime(10, 0)), error));

    OperationLogFilter filter;
    filter.adminId = 1;
    QList<OperationLogEntry> logs;
    QVERIFY2(service.query(filter, logs, error), qPrintable(error));
    QCOMPARE(logs.size(), 2);
    QCOMPARE(logs.first().operationFunc, QString("screenshot"));

    QVERIFY(!service.deleteRange(QDateTime(QDate(2026, 9, 22), QTime(7, 0)),
                                 QDateTime(QDate(2026, 9, 22), QTime(8, 30)),
                                 false, error));
    QVERIFY(error.contains(QString::fromUtf8("确认")));
    QVERIFY2(service.deleteRange(QDateTime(QDate(2026, 9, 22), QTime(7, 0)),
                                 QDateTime(QDate(2026, 9, 22), QTime(8, 30)),
                                 true, error), qPrintable(error));
    QVERIFY(service.query(filter, logs, error));
    QCOMPARE(logs.size(), 1);
    QCOMPARE(logs.first().operationFunc, QString("screenshot"));
}

QTEST_GUILESS_MAIN(Day7Day8Tests)
#include "tst_day7_day8.moc"
