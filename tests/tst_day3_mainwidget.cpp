#include <QtTest>

#include "../view/mainwidget.h"

#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>

class Day3MainWidgetTests : public QObject
{
    Q_OBJECT

private slots:
    void monitorPageStartsWithFourVisibleChannels();
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
