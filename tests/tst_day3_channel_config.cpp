#include <QtTest>

#include "../model/channelconfig.h"
#include "../service/channelconfigpolicy.h"
#include "../service/channelconfigservice.h"
#include "../util/dbconn.h"

class Day3ChannelConfigTests : public QObject
{
    Q_OBJECT

private slots:
    void defaultsMatchDay3Plan();
    void duplicateCameraAssignmentIsRejected();
    void emptyFileSourceIsRejectedWhenStarting();
    void channelConfigRoundTripsThroughJson();
    void channelConfigsRoundTripThroughDatabase();
};

void Day3ChannelConfigTests::defaultsMatchDay3Plan()
{
    const QList<ChannelConfig> configs = ChannelConfigPolicy::defaults();

    QCOMPARE(configs.size(), 4);
    QCOMPARE(configs.at(0).channelId, 1);
    QCOMPARE(configs.at(0).name, QStringLiteral("通道 1"));
    QCOMPARE(configs.at(0).sourceType, ChannelSourceType::Camera);
    QCOMPARE(configs.at(0).cameraIndex, 0);

    for (int i = 1; i < configs.size(); ++i) {
        QCOMPARE(configs.at(i).channelId, i + 1);
        QCOMPARE(configs.at(i).sourceType, ChannelSourceType::File);
        QVERIFY(configs.at(i).filePath.isEmpty());
    }
}

void Day3ChannelConfigTests::duplicateCameraAssignmentIsRejected()
{
    QList<ChannelConfig> configs = ChannelConfigPolicy::defaults();
    configs[1].sourceType = ChannelSourceType::Camera;
    configs[1].cameraIndex = 0;

    QString errorText;
    QVERIFY(!ChannelConfigPolicy::validateAssignments(configs, errorText));
    QVERIFY(errorText.contains(QStringLiteral("摄像头 0")));
}

void Day3ChannelConfigTests::emptyFileSourceIsRejectedWhenStarting()
{
    const ChannelConfig config = ChannelConfigPolicy::defaults().at(1);

    QString errorText;
    QVERIFY(!ChannelConfigPolicy::validateForStart(config, errorText));
    QVERIFY(errorText.contains(QStringLiteral("视频文件")));
}

void Day3ChannelConfigTests::channelConfigRoundTripsThroughJson()
{
    ChannelConfig input;
    input.channelId = 3;
    input.name = QStringLiteral("停车场");
    input.sourceType = ChannelSourceType::File;
    input.cameraIndex = -1;
    input.filePath = QStringLiteral("C:/videos/parking.mp4");

    const QString json = ChannelConfigPolicy::encode(input);
    ChannelConfig output;
    QString errorText;

    QVERIFY(ChannelConfigPolicy::decode(json, output, errorText));
    QCOMPARE(output, input);
}

void Day3ChannelConfigTests::channelConfigsRoundTripThroughDatabase()
{
    DbConn *connection = DbConn::getInstance();
    QVERIFY2(connection != nullptr && connection->isOpen(),
             qPrintable(connection == nullptr
                        ? QStringLiteral("数据库连接对象为空")
                        : connection->lastErrorText()));
    ChannelConfigService service;
    QList<ChannelConfig> original;
    QString errorText;
    QVERIFY2(service.load(original, errorText), qPrintable(errorText));

    QList<ChannelConfig> expected = ChannelConfigPolicy::defaults();
    expected[0].name = QStringLiteral("前门");
    expected[1].filePath = QStringLiteral("C:/videos/west.mp4");

    QVERIFY2(service.save(expected, errorText), qPrintable(errorText));

    QList<ChannelConfig> actual;
    QVERIFY2(service.load(actual, errorText), qPrintable(errorText));
    QCOMPARE(actual, expected);

    QVERIFY2(service.save(original, errorText), qPrintable(errorText));
}

QTEST_GUILESS_MAIN(Day3ChannelConfigTests)
#include "tst_day3_channel_config.moc"
