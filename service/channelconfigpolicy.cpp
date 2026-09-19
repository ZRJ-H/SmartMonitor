#include "channelconfigpolicy.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>

QList<ChannelConfig> ChannelConfigPolicy::defaults()
{
    QList<ChannelConfig> configs;
    for (int channelId = 1; channelId <= 4; ++channelId) {
        ChannelConfig config;
        config.channelId = channelId;
        config.name = QStringLiteral("通道 %1").arg(channelId);
        config.sourceType = channelId == 1
                ? ChannelSourceType::Camera
                : ChannelSourceType::File;
        config.cameraIndex = channelId == 1 ? 0 : -1;
        configs.append(config);
    }
    return configs;
}

bool ChannelConfigPolicy::validateAssignments(
        const QList<ChannelConfig> &configs,
        QString &errorText)
{
    errorText.clear();
    if (configs.size() != 4) {
        errorText = QStringLiteral("必须配置 4 个监控通道");
        return false;
    }

    QSet<int> channelIds;
    QSet<int> cameraIndexes;
    for (const ChannelConfig &config : configs) {
        if (config.channelId < 1 || config.channelId > 4
                || channelIds.contains(config.channelId)) {
            errorText = QStringLiteral("通道编号必须是互不重复的 1-4");
            return false;
        }
        channelIds.insert(config.channelId);

        if (config.name.trimmed().isEmpty()) {
            errorText = QStringLiteral("通道 %1 的名称不能为空")
                    .arg(config.channelId);
            return false;
        }

        if (config.sourceType == ChannelSourceType::Camera) {
            if (config.cameraIndex < 0) {
                errorText = QStringLiteral("通道 %1 的摄像头编号无效")
                        .arg(config.channelId);
                return false;
            }
            if (cameraIndexes.contains(config.cameraIndex)) {
                errorText = QStringLiteral("摄像头 %1 已被其他通道占用")
                        .arg(config.cameraIndex);
                return false;
            }
            cameraIndexes.insert(config.cameraIndex);
        }
    }
    return true;
}

bool ChannelConfigPolicy::validateForStart(const ChannelConfig &config,
                                            QString &errorText)
{
    errorText.clear();
    if (config.sourceType == ChannelSourceType::Camera) {
        if (config.cameraIndex < 0) {
            errorText = QStringLiteral("请选择可用摄像头");
            return false;
        }
        return true;
    }

    if (config.filePath.trimmed().isEmpty()) {
        errorText = QStringLiteral("请选择本地视频文件");
        return false;
    }
    return true;
}

QString ChannelConfigPolicy::encode(const ChannelConfig &config)
{
    QJsonObject object;
    object.insert(QStringLiteral("channelId"), config.channelId);
    object.insert(QStringLiteral("name"), config.name);
    object.insert(QStringLiteral("sourceType"),
                  config.sourceType == ChannelSourceType::Camera
                  ? QStringLiteral("camera")
                  : QStringLiteral("file"));
    object.insert(QStringLiteral("cameraIndex"), config.cameraIndex);
    object.insert(QStringLiteral("filePath"), config.filePath);
    return QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::Compact));
}

bool ChannelConfigPolicy::decode(const QString &json,
                                 ChannelConfig &config,
                                 QString &errorText)
{
    errorText.clear();
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(
                json.toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        errorText = QStringLiteral("通道配置格式损坏");
        return false;
    }

    const QJsonObject object = document.object();
    const QString sourceType = object.value(QStringLiteral("sourceType")).toString();
    if (sourceType != QStringLiteral("camera")
            && sourceType != QStringLiteral("file")) {
        errorText = QStringLiteral("通道来源类型无效");
        return false;
    }

    ChannelConfig decoded;
    decoded.channelId = object.value(QStringLiteral("channelId")).toInt();
    decoded.name = object.value(QStringLiteral("name")).toString();
    decoded.sourceType = sourceType == QStringLiteral("camera")
            ? ChannelSourceType::Camera
            : ChannelSourceType::File;
    decoded.cameraIndex = object.value(QStringLiteral("cameraIndex")).toInt(-1);
    decoded.filePath = object.value(QStringLiteral("filePath")).toString();

    if (decoded.channelId < 1 || decoded.channelId > 4
            || decoded.name.trimmed().isEmpty()) {
        errorText = QStringLiteral("通道配置内容不完整");
        return false;
    }

    config = decoded;
    return true;
}
