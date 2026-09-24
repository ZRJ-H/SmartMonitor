#include <QtTest>
#include <QGraphicsView>
#include <QFontDatabase>
#include "../view/apptheme.h"

#include "../view/imagepage.h"

#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QSignalSpy>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTableWidget>
#include <QTemporaryDir>

class Day7Day8UiTests : public QObject
{
    Q_OBJECT

private slots:
    void keyboardFocusWithinMultiSelectionUpdatesLinkedVideo() {
        QTemporaryDir dir; const auto start=QDateTime::currentDateTime();
        for(int i=0;i<2;++i) QVERIFY(insertImage(QString::number(i),dir.filePath(QString::number(i)),start.addSecs(i),100+i,i*1000)>0);
        ImagePage page(database,dir.path());page.reload();page.show();QTest::qWait(20);
        auto list=page.findChild<QListWidget*>("imageThumbnails");
        QTest::mouseClick(list->viewport(),Qt::LeftButton,Qt::NoModifier,list->visualItemRect(list->item(0)).center());
        QTest::mouseClick(list->viewport(),Qt::LeftButton,Qt::ControlModifier,list->visualItemRect(list->item(1)).center());
        QCOMPARE(list->selectedItems().size(),2);QCOMPARE(list->currentRow(),1);
        QTest::keyClick(list,Qt::Key_Left,Qt::ControlModifier);
        QCOMPARE(list->selectedItems().size(),2);QCOMPARE(list->currentRow(),0);
        QSignalSpy spy(&page,&ImagePage::playbackRequested);
        page.findChild<QPushButton*>("playbackImageButton")->click();
        QCOMPARE(spy.count(),1);QCOMPARE(spy[0][0].toLongLong(),qint64(101));
    }


    void thumbnailClickOpensTheSelectedVideo() {
        QTemporaryDir dir;
        QVERIFY(insertImage("linked.jpg",dir.filePath("linked.jpg"),QDateTime::currentDateTime(),23,4567)>0);
        ImagePage page(database,dir.path());page.reload();page.show();QTest::qWait(20);
        auto list=page.findChild<QListWidget*>("imageThumbnails");QVERIFY(list);
        QSignalSpy spy(&page,&ImagePage::playbackRequested);
        QTest::mouseClick(list->viewport(),Qt::LeftButton,Qt::NoModifier,list->visualItemRect(list->item(0)).center());
        auto button=page.findChild<QPushButton*>("playbackImageButton");QVERIFY(button->isEnabled());button->click();
        QCOMPARE(spy.count(),1);QCOMPARE(spy[0][0].toLongLong(),qint64(23));QCOMPARE(spy[0][1].toLongLong(),qint64(4567));
    }
    void deselectingThumbnailDisablesActionsAndCannotPlayStaleVideo() {
        QTemporaryDir dir;
        QVERIFY(insertImage("linked.jpg",dir.filePath("linked.jpg"),QDateTime::currentDateTime(),23,4567)>0);
        ImagePage page(database,dir.path());page.reload();page.show();QTest::qWait(20);
        auto list=page.findChild<QListWidget*>("imageThumbnails");
        const auto point=list->visualItemRect(list->item(0)).center();
        QTest::mouseClick(list->viewport(),Qt::LeftButton,Qt::NoModifier,point);
        QTest::mouseClick(list->viewport(),Qt::LeftButton,Qt::ControlModifier,point);
        QCOMPARE(list->selectedItems().size(),0);
        QVERIFY(!page.findChild<QPushButton*>("deleteImageButton")->isEnabled());
        QVERIFY(!page.findChild<QPushButton*>("exportImageButton")->isEnabled());
        QVERIFY(!page.findChild<QPushButton*>("playbackImageButton")->isEnabled());
        QSignalSpy spy(&page,&ImagePage::playbackRequested);page.requestSelectedPlayback();QCOMPARE(spy.count(),0);
    }
    void selectingSameRowAfterPageChangeUsesNewImage() {
        QTemporaryDir dir; const auto start=QDateTime::currentDateTime();
        for(int i=0;i<10;++i) QVERIFY(insertImage(QString::number(i),dir.filePath(QString::number(i)),start.addSecs(i),100+i,i*1000)>0);
        ImagePage page(database,dir.path());page.reload();page.show();QTest::qWait(20);
        auto list=page.findChild<QListWidget*>("imageThumbnails");
        QTest::mouseClick(list->viewport(),Qt::LeftButton,Qt::NoModifier,list->visualItemRect(list->item(0)).center());
        page.findChild<QPushButton*>("imageNextButton")->click();QTest::qWait(20);
        QCOMPARE(list->count(),1);
        QTest::mouseClick(list->viewport(),Qt::LeftButton,Qt::NoModifier,list->visualItemRect(list->item(0)).center());
        auto play=page.findChild<QPushButton*>("playbackImageButton");QVERIFY(play->isEnabled());
        QSignalSpy spy(&page,&ImagePage::playbackRequested);play->click();
        QCOMPARE(spy.count(),1);QCOMPARE(spy[0][0].toLongLong(),qint64(100));
    }

    void init();
    void cleanup();
    void imagePageLoadsNewestRowsAndPaginates();
    void selectingVideoImageEmitsPlaybackRequest();
    void missingImageShowsPlaceholderAndKeepsDeleteAvailable();

private:
    qint64 insertImage(const QString &name, const QString &path,
                       const QDateTime &capturedAt, qint64 videoId = 0,
                       qint64 offsetMs = 0);

    QSqlDatabase database;
    QString connectionName;
};

void Day7Day8UiTests::init()
{
    connectionName = QStringLiteral("day78_ui_%1")
            .arg(reinterpret_cast<quintptr>(this));
    database = QSqlDatabase::addDatabase("QSQLITE", connectionName);
    database.setDatabaseName(":memory:");
    QVERIFY(database.open());

    QSqlQuery query(database);
    QVERIFY(query.exec("CREATE TABLE feature_image (id INTEGER PRIMARY KEY AUTOINCREMENT,"
                       "image_name TEXT NOT NULL,image_path TEXT NOT NULL UNIQUE,"
                       "channel_id INTEGER NOT NULL,capture_time TEXT NOT NULL,"
                       "capture_source TEXT NOT NULL,exception_id INTEGER,video_id INTEGER,"
                       "video_offset_ms INTEGER)"));
}

void Day7Day8UiTests::cleanup()
{
    database.close();
    database = QSqlDatabase();
    QSqlDatabase::removeDatabase(connectionName);
}

qint64 Day7Day8UiTests::insertImage(const QString &name, const QString &path,
                                    const QDateTime &capturedAt,
                                    qint64 videoId, qint64 offsetMs)
{
    QSqlQuery query(database);
    query.prepare("INSERT INTO feature_image(image_name,image_path,channel_id,capture_time,"
                  "capture_source,exception_id,video_id,video_offset_ms) "
                  "VALUES(:name,:path,1,:time,'manual',NULL,:video,:offset)");
    query.bindValue(":name", name);
    query.bindValue(":path", path);
    query.bindValue(":time", capturedAt);
    query.bindValue(":video", videoId > 0 ? QVariant(videoId) : QVariant());
    query.bindValue(":offset", videoId > 0 ? QVariant(offsetMs) : QVariant());
    if (!query.exec()) return 0;
    return query.lastInsertId().toLongLong();
}

void Day7Day8UiTests::imagePageLoadsNewestRowsAndPaginates()
{
    QTemporaryDir directory;
    const QDateTime start(QDate(2026, 9, 22), QTime(8, 0));
    for (int index = 0; index < 21; ++index) {
        const QString name = QStringLiteral("image-%1.jpg").arg(index, 2, 10, QLatin1Char('0'));
        QVERIFY(insertImage(name, directory.filePath(name), start.addSecs(index)) > 0);
    }

    ImagePage page(database, directory.path());
    page.reload();
    QTableWidget *table = page.findChild<QTableWidget *>("imageTable");
    QVERIFY(table != nullptr);
    QCOMPARE(table->rowCount(), 9);
    auto thumbnails=page.findChild<QListWidget*>("imageThumbnails");
    QVERIFY(thumbnails);QCOMPARE(thumbnails->count(),9);
    QCOMPARE(table->item(0, 0)->text(), QString("image-20.jpg"));

    QVERIFY(QMetaObject::invokeMethod(&page, "nextPage"));
    QCOMPARE(table->rowCount(), 9);
    QVERIFY(QMetaObject::invokeMethod(&page, "nextPage"));
    QCOMPARE(table->rowCount(), 3);
    QCOMPARE(table->item(0, 0)->text(), QString("image-02.jpg"));
}

void Day7Day8UiTests::selectingVideoImageEmitsPlaybackRequest()
{
    QTemporaryDir directory;
    const QString path = directory.filePath("linked.jpg");
    QVERIFY(insertImage("linked.jpg", path,
                        QDateTime(QDate(2026, 9, 22), QTime(9, 0)),
                        23, 4567) > 0);

    ImagePage page(database, directory.path());
    page.reload();
    QTableWidget *table = page.findChild<QTableWidget *>("imageTable");
    QVERIFY(table != nullptr);
    table->selectRow(0);

    QSignalSpy spy(&page, &ImagePage::playbackRequested);
    QVERIFY(QMetaObject::invokeMethod(&page, "requestSelectedPlayback"));
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toLongLong(), qint64(23));
    QCOMPARE(spy.at(0).at(1).toLongLong(), qint64(4567));
}

void Day7Day8UiTests::missingImageShowsPlaceholderAndKeepsDeleteAvailable()
{
    QTemporaryDir directory;
    const QString missingPath = directory.filePath("missing.jpg");
    QVERIFY(insertImage("missing.jpg", missingPath,
                        QDateTime(QDate(2026, 9, 22), QTime(10, 0))) > 0);

    ImagePage page(database, directory.path());
    page.reload();
    QTableWidget *table = page.findChild<QTableWidget *>("imageTable");
    QLabel *preview = page.findChild<QLabel *>("imageSelectionStatus");
    QPushButton *deleteButton = page.findChild<QPushButton *>("deleteImageButton");
    QVERIFY(table != nullptr);
    QVERIFY(preview != nullptr);
    QVERIFY(page.findChild<QLabel*>("imagePreview")==nullptr);
    QVERIFY(deleteButton != nullptr);

    table->selectRow(0);
    QCoreApplication::processEvents();
    QCOMPARE(preview->text(), QString::fromUtf8("图片文件不存在"));
    QVERIFY(deleteButton->isEnabled());
    page.resize(1100,650);page.show();QTest::qWait(30);
    auto grid=page.findChild<QListWidget*>("imageThumbnails");QVERIFY(grid);
    QVERIFY(grid->width()>page.width()*0.8);
    QVERIFY(QMetaObject::invokeMethod(&page,"openViewer"));
    auto viewer=page.findChild<QGraphicsView*>("largeImageView");QVERIFY(viewer);
    QVERIFY(viewer->window()->isVisible());
    const auto capture=qEnvironmentVariable("SMART_MONITOR_UI_CAPTURE_DIR");
    if(!capture.isEmpty()){
        // offscreen 不自动加载 Windows 中文字体；截图应用产品主题并使用真实 JPG 缩略图。
        QFontDatabase::addApplicationFont("C:/Windows/Fonts/msyh.ttc");
        AppTheme::applyMain(&page);
        for(int i=0;i<8;++i){
            const QString path=directory.filePath(QString("sample-%1.jpg").arg(i));
            QImage sample(640,360,QImage::Format_RGB32);sample.fill(QColor::fromHsv(i*35,140,180));
            QVERIFY(sample.save(path));QVERIFY(insertImage(QString("预览样本 %1").arg(i+1),path,QDateTime::currentDateTime().addSecs(-i))>0);
        }
        viewer->window()->close();page.reload();QTest::qWait(40);
        QDir().mkpath(capture);page.grab().save(QDir(capture).filePath("review-images.png"));
    }
}

QTEST_MAIN(Day7Day8UiTests)
#include "tst_day7_day8_ui.moc"
