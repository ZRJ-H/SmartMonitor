#include <QtTest>
#include <QClipboard>
#include <QDialog>
#include <QFontDatabase>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSqlQuery>
#include <QSqlError>
#include <QTabWidget>
#include <QTableWidget>
#include <QScopeGuard>
#include "../view/operationlogpage.h"
#include "../view/apptheme.h"

class OperationLogUiTests : public QObject {
    Q_OBJECT
    QSqlDatabase db;
    QString connection;
    qint64 insertLog(const QString &description) {
        OperationLogEntry entry;
        entry.adminId=3;entry.operationTime=QDateTime(QDate(2026,9,24),QTime(15,19,46));
        entry.operationFunc="<b>回放</b>";entry.operationDesc=description;
        OperationLogModel model(db);QString error;
        return model.insert(entry,error)?entry.id:-1;
    }
private slots:
    void init() {
        connection=QString("log-ui-%1").arg(reinterpret_cast<quintptr>(this));
        db=QSqlDatabase::addDatabase("QSQLITE",connection);db.setDatabaseName(":memory:");QVERIFY(db.open());
        QSqlQuery q(db);
        QVERIFY(q.exec("CREATE TABLE operation_log(id INTEGER PRIMARY KEY,admin_id INTEGER,operation_time TEXT,operation_func TEXT,operation_desc TEXT)"));
        QVERIFY(q.exec("CREATE TABLE exception_log(id INTEGER PRIMARY KEY,channel_id INTEGER,event_time TEXT,event_status TEXT,related_video_path TEXT)"));
        QVERIFY(q.exec("CREATE TABLE event_media_link(exception_id INTEGER,video_id INTEGER)"));
        QVERIFY(q.exec("CREATE TABLE video(id INTEGER PRIMARY KEY,video_path TEXT)"));
        QFontDatabase::addApplicationFont("C:/Windows/Fonts/msyh.ttc");
    }
    void cleanup() {db.close();db=QSqlDatabase();QSqlDatabase::removeDatabase(connection);}

    void buttonShowsFullPlainTextAndCopiesIt() {
        const QString description="C:/监控记录/"+QString("very-long-path/").repeated(40)+"录像.mp4\n<b>原样显示</b>\n第二行详情";
        const auto id=insertLog(description);QVERIFY(id>0);
        OperationLogPage page(db);AppTheme::applyMain(&page);page.resize(1000,650);page.reload();page.show();
        auto button=page.findChild<QPushButton*>("viewLogDetailsButton");QVERIFY(button);QVERIFY(!button->isEnabled());
        auto table=page.findChild<QTableWidget*>("operationLogTable");table->selectRow(0);QVERIFY(button->isEnabled());
        button->click();
        auto dialog=page.findChild<QDialog*>("operationLogDetailsDialog");QVERIFY(dialog);QVERIFY(dialog->isVisible());
        QCOMPARE(dialog->windowModality(),Qt::WindowModal);
        auto text=dialog->findChild<QPlainTextEdit*>("operationLogDetailsText");QVERIFY(text);QVERIFY(text->isReadOnly());
        QCOMPARE(text->toPlainText(),description);
        QCOMPARE(dialog->findChild<QLabel*>("operationLogId")->text(),QString::number(id));
        QCOMPARE(dialog->findChild<QLabel*>("operationLogFunction")->text(),QString("<b>回放</b>"));
        QCOMPARE(dialog->findChild<QLabel*>("operationLogFunction")->textFormat(),Qt::PlainText);
        auto clipboard=QApplication::clipboard();const QString previous=clipboard->text();
        const auto restore=qScopeGuard([&]{clipboard->setText(previous);});
        dialog->findChild<QPushButton*>("copyLogDetailsButton")->click();QCOMPARE(clipboard->text(),description);
        const QString captureDir=qEnvironmentVariable("SMART_MONITOR_UI_CAPTURE_DIR");
        if(!captureDir.isEmpty()) {QDir().mkpath(captureDir);QTest::qWait(30);dialog->grab().save(QDir(captureDir).filePath("log-details.png"));}
        dialog->close();QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);
        QVERIFY(!page.findChild<QDialog*>("operationLogDetailsDialog"));
    }
    void rowActivationShowsTheCorrectEntry_data() {
        QTest::addColumn<bool>("keyboard");QTest::newRow("double-click")<<false;QTest::newRow("enter")<<true;
    }
    void rowActivationShowsTheCorrectEntry() {
        QFETCH(bool,keyboard);
        QVERIFY(insertLog("older entry")>0);QVERIFY(insertLog("newer entry")>0);
        OperationLogPage page(db);page.resize(1000,650);page.reload();page.show();QTest::qWait(20);
        auto table=page.findChild<QTableWidget*>("operationLogTable");
        table->setCurrentCell(1,4);table->selectRow(1);table->setFocus();
        if(keyboard) QTest::keyClick(table,Qt::Key_Return);
        else {
            const auto position=table->visualItemRect(table->item(1,4)).center();
            // QTest::mouseDClick 只发双击事件；真实双击之前还有一次完整按下/释放。
            QTest::mouseClick(table->viewport(),Qt::LeftButton,Qt::NoModifier,position);
            QTest::mouseDClick(table->viewport(),Qt::LeftButton,Qt::NoModifier,position);
        }
        QTRY_VERIFY(page.findChild<QDialog*>("operationLogDetailsDialog"));
        auto dialog=page.findChild<QDialog*>("operationLogDetailsDialog");
        QCOMPARE(dialog->findChild<QPlainTextEdit*>("operationLogDetailsText")->toPlainText(),QString("older entry"));
        dialog->close();
    }
    void deselectionTabChangeAndDisabledPageCannotOpenDetails() {
        QVERIFY(insertLog("private detail")>0);
        OperationLogPage page(db);page.reload();page.show();
        auto table=page.findChild<QTableWidget*>("operationLogTable");
        auto button=page.findChild<QPushButton*>("viewLogDetailsButton");QVERIFY(button);
        table->selectRow(0);QVERIFY(button->isEnabled());table->clearSelection();QVERIFY(!button->isEnabled());
        QVERIFY(QMetaObject::invokeMethod(&page,"showSelectedDetails"));QVERIFY(!page.findChild<QDialog*>());
        table->selectRow(0);page.findChild<QTabWidget*>()->setCurrentIndex(1);QVERIFY(!button->isEnabled());
        QVERIFY(QMetaObject::invokeMethod(&page,"showSelectedDetails"));QVERIFY(!page.findChild<QDialog*>());
        page.findChild<QTabWidget*>()->setCurrentIndex(0);table->selectRow(0);button->click();
        auto dialog=page.findChild<QDialog*>("operationLogDetailsDialog");QVERIFY(dialog);QVERIFY(dialog->isVisible());
        page.setEnabled(false);QVERIFY(!dialog->isVisible());
        QVERIFY(QMetaObject::invokeMethod(&page,"showSelectedDetails"));
    }
    void reloadClearsSelectionAndEmptyDescriptionStillHasMetadata() {
        const auto id=insertLog("");QVERIFY(id>0);
        OperationLogPage page(db);page.reload();page.show();
        auto table=page.findChild<QTableWidget*>("operationLogTable");
        auto button=page.findChild<QPushButton*>("viewLogDetailsButton");QVERIFY(button);
        table->selectRow(0);page.reload();QVERIFY(!button->isEnabled());
        table->selectRow(0);button->click();
        auto dialog=page.findChild<QDialog*>("operationLogDetailsDialog");QVERIFY(dialog);
        QCOMPARE(dialog->findChild<QLabel*>("operationLogId")->text(),QString::number(id));
        QVERIFY(dialog->findChild<QPlainTextEdit*>("operationLogDetailsText")->toPlainText().isEmpty());
        QVERIFY(!dialog->findChild<QPushButton*>("copyLogDetailsButton")->isEnabled());
        dialog->close();
    }
};
QTEST_MAIN(OperationLogUiTests)
#include "tst_operation_log_ui.moc"
