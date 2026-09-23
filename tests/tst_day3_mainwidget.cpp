// 主窗口界面测试：验证启动后的四画面布局及通道控制、四行配置控件是否齐全。
#include <QtTest>
#include "testdatabase.h"

#include "../view/mainwidget.h"

#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>

class Day3MainWidgetTests : public QObject
{
    Q_OBJECT

private slots:
    void managementPagesRequireLoginAndExposeRealControls() {
        MainWidget widget;
        auto imageButton=widget.findChild<QPushButton*>("imagesNavigation");
        auto logButton=widget.findChild<QPushButton*>("logsNavigation");
        QVERIFY(imageButton); QVERIFY(logButton);
        QVERIFY(!imageButton->isEnabled()); QVERIFY(!logButton->isEnabled());
        QVERIFY(widget.findChild<QLineEdit*>("imageDirectoryEdit"));
        widget.setLoggedInUser(17,"admin17");
        QVERIFY(imageButton->isEnabled()); QVERIFY(logButton->isEnabled());
        imageButton->click();
        QVERIFY(widget.findChild<QWidget*>("imagePage")->isEnabled());
        logButton->click();
        QVERIFY(widget.findChild<QWidget*>("operationLogPage")->isEnabled());
    }

    void initTestCase() { QVERIFY(initializeTestDatabase()); }
    void cleanupTestCase() { DbConn::releaseDbConn(); }
    // 验证监控页初始显示四个可见视频窗口。
    void monitorPageStartsWithFourVisibleChannels();
    // 验证通道操作按钮和四组配置输入控件均已创建。
    void channelControlsAndFourConfigRowsArePresent();
};

void Day3MainWidgetTests::monitorPageStartsWithFourVisibleChannels()
{
    MainWidget widget;
    widget.show();
    QCoreApplication::processEvents();

    const QList<QLabel *> windows =
            widget.findChildren<QLabel *>(QStringLiteral("videoWindow"));
    QCOMPARE(windows.size(), 4);
    for (QLabel *window : windows) {
        QVERIFY(!window->isHidden());
    }
}

void Day3MainWidgetTests::channelControlsAndFourConfigRowsArePresent()
{
    MainWidget widget;

    QListWidget *deviceList =
            widget.findChild<QListWidget *>(QStringLiteral("deviceList"));
    QVERIFY(deviceList != nullptr);
    QCOMPARE(deviceList->count(), 4);

    QVERIFY(widget.findChild<QPushButton *>(
                QStringLiteral("startPreviewButton")) != nullptr);
    QVERIFY(widget.findChild<QPushButton *>(
                QStringLiteral("stopPreviewButton")) != nullptr);
    QVERIFY(widget.findChild<QPushButton *>(
                QStringLiteral("refreshCamerasButton")) != nullptr);
    QVERIFY(widget.findChild<QPushButton *>(
                QStringLiteral("saveChannelConfigButton")) != nullptr);

    for (int channelId = 1; channelId <= 4; ++channelId) {
        QVERIFY(widget.findChild<QLineEdit *>(
                    QStringLiteral("channelNameEdit_%1").arg(channelId))
                != nullptr);
    }
}

QTEST_MAIN(Day3MainWidgetTests)
#include "tst_day3_mainwidget.moc"
