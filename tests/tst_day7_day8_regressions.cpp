#include <QtTest>

#include "../service/imageservice.h"
#include "../service/operationlogservice.h"

#include <QFile>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>

class Day7Day8RegressionTests : public QObject
{
    Q_OBJECT
private slots:
    void init();
    void cleanup();
    void duplicateManualCaptureKeepsOriginalFileAndRow();
    void distinctEventsInSameSecondDoNotCollide();
    void repeatedEventDoesNotCreateMoreThanThreeImages();
    void logsArePagedDeterministically();
    void adoptsMatchingOrphanEventImage();
private:
    QSqlDatabase database;
    QString connectionName;
};

void Day7Day8RegressionTests::init()
{
    connectionName = QStringLiteral("day78_reg_%1").arg(reinterpret_cast<quintptr>(this));
    database = QSqlDatabase::addDatabase("QSQLITE", connectionName);
    database.setDatabaseName(":memory:");
    QVERIFY(database.open());
    QSqlQuery query(database);
    QVERIFY(query.exec("CREATE TABLE feature_image (id INTEGER PRIMARY KEY AUTOINCREMENT,"
                       "image_name TEXT NOT NULL,image_path TEXT NOT NULL UNIQUE,"
                       "channel_id INTEGER NOT NULL,capture_time TEXT NOT NULL,"
                       "capture_source TEXT NOT NULL,exception_id INTEGER,video_id INTEGER,"
                       "video_offset_ms INTEGER)"));
    QVERIFY(query.exec("CREATE TABLE operation_log (id INTEGER PRIMARY KEY AUTOINCREMENT,"
                       "admin_id INTEGER NOT NULL,operation_time TEXT NOT NULL,"
                       "operation_func TEXT NOT NULL,operation_desc TEXT)"));
}

void Day7Day8RegressionTests::cleanup()
{
    database.close();
    database = QSqlDatabase();
    QSqlDatabase::removeDatabase(connectionName);
}

void Day7Day8RegressionTests::duplicateManualCaptureKeepsOriginalFileAndRow()
{
    QTemporaryDir directory;
    ImageService service(database, directory.path());
    const QDateTime capturedAt(QDate(2026, 9, 22), QTime(11, 2, 3, 456));
    QImage firstFrame(8, 8, QImage::Format_RGB32);
    firstFrame.fill(Qt::red);
    QImage secondFrame(8, 8, QImage::Format_RGB32);
    secondFrame.fill(Qt::blue);
    FeatureImage first;
    FeatureImage second;
    QString error;
    QVERIFY(service.saveManualImage(firstFrame, 2, capturedAt, 0, 0, first, error));
    QFile original(first.imagePath);
    QVERIFY(original.open(QIODevice::ReadOnly));
    const QByteArray bytes = original.readAll();
    original.close();

    QVERIFY(!service.saveManualImage(secondFrame, 2, capturedAt, 0, 0, second, error));
    QVERIFY(QFileInfo::exists(first.imagePath));
    QVERIFY(original.open(QIODevice::ReadOnly));
    QCOMPARE(original.readAll(), bytes);
    QSqlQuery count(database);
    QVERIFY(count.exec("SELECT COUNT(*) FROM feature_image"));
    QVERIFY(count.next());
    QCOMPARE(count.value(0).toInt(), 1);
}

void Day7Day8RegressionTests::distinctEventsInSameSecondDoNotCollide()
{
    QTemporaryDir directory;
    ImageService service(database, directory.path());
    const QDateTime capturedAt(QDate(2026, 9, 22), QTime(12, 0));
    const QList<QImage> frames(3, QImage(8, 8, QImage::Format_RGB32));
    QList<FeatureImage> first;
    QList<FeatureImage> second;
    QString error;
    QVERIFY(service.saveEventImages(frames, 1, 100, capturedAt, first, error));
    QVERIFY(service.saveEventImages(frames, 1, 101, capturedAt, second, error));
    QCOMPARE(first.first().imageName, second.first().imageName);
    QVERIFY(first.first().imagePath != second.first().imagePath);
}

void Day7Day8RegressionTests::repeatedEventDoesNotCreateMoreThanThreeImages()
{
    QTemporaryDir directory;
    ImageService service(database, directory.path());
    const QList<QImage> frames(3, QImage(8, 8, QImage::Format_RGB32));
    QList<FeatureImage> first;
    QList<FeatureImage> retry;
    QString error;
    QVERIFY(service.saveEventImages(frames, 3, 200,
                                    QDateTime(QDate(2026, 9, 22), QTime(13, 0)),
                                    first, error));
    QVERIFY(!service.saveEventImages(frames, 3, 200,
                                     QDateTime(QDate(2026, 9, 22), QTime(13, 1)),
                                     retry, error));
    QSqlQuery count(database);
    QVERIFY(count.exec("SELECT COUNT(*) FROM feature_image WHERE exception_id=200"));
    QVERIFY(count.next());
    QCOMPARE(count.value(0).toInt(), 3);
}

void Day7Day8RegressionTests::logsArePagedDeterministically()
{
    OperationLogService service(database);
    const QDateTime sameTime(QDate(2026, 9, 22), QTime(15, 0));
    QString error;
    for (int index = 1; index <= 5; ++index) {
        QVERIFY(service.record(1, QStringLiteral("op%1").arg(index), "", sameTime, error));
    }
    OperationLogFilter filter;
    QList<OperationLogEntry> firstPage;
    QList<OperationLogEntry> secondPage;
    QVERIFY(service.query(filter, firstPage, error, 1, 2));
    QVERIFY(service.query(filter, secondPage, error, 2, 2));
    QCOMPARE(firstPage.at(0).operationFunc, QString("op5"));
    QCOMPARE(firstPage.at(1).operationFunc, QString("op4"));
    QCOMPARE(secondPage.at(0).operationFunc, QString("op3"));
    QCOMPARE(secondPage.at(1).operationFunc, QString("op2"));
}

void Day7Day8RegressionTests::adoptsMatchingOrphanEventImage() {
    QTemporaryDir dir; ImageService service(database,dir.path());
    const QDateTime captured(QDate(2026,9,23),QTime(12,0));
    QImage frame(8,8,QImage::Format_RGB32);frame.fill(Qt::green);
    const QString folder=dir.filePath("event-55");QVERIFY(QDir().mkpath(folder));
    const QString orphan=QDir(folder).filePath("异常_通道1_20260923120000_1.jpg");
    QVERIFY(frame.save(orphan,"JPG",92));
    QList<FeatureImage> images;QString error;
    QVERIFY2(service.saveEventImages({frame,frame,frame},1,55,captured,images,error),qPrintable(error));
    QCOMPARE(images.size(),3);QVERIFY(QFile::exists(orphan));
}

QTEST_GUILESS_MAIN(Day7Day8RegressionTests)
#include "tst_day7_day8_regressions.moc"
