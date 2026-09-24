// 【阅读导引】
// 系统配置业务：校验目录/范围/通道、兼容两套历史键，再交给 Model 成组保存。
// 相对存储目录按程序目录解析；保存成功不等于设备一定能重新启动。

#include "systemconfigservice.h"

#include <QDir>
#include <QFile>
#include <QUuid>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QCoreApplication>

static QString storagePath(const QString &path) {
    return QDir::cleanPath(QDir(QCoreApplication::applicationDirPath()).absoluteFilePath(path));
}

// 构造对象并初始化所需成员；冒号后的初始化列表先于函数体执行。
SystemConfigService::SystemConfigService(const QSqlDatabase &database)
    : model(database)
{
}

// 先读取兼容键，再用实际工程优先键覆盖；在临时对象校验成功后才返回配置。
bool SystemConfigService::load(SystemConfig &config, QString &errorText) const
{
    QMap<QString, QString> values;
    if (!model.loadAll(values, errorText)) return false;
    SystemConfig loaded;
    if (values.contains("video_dir")) loaded.videoDirectory = values.value("video_dir");
    if (values.contains("image_dir")) loaded.imageDirectory = values.value("image_dir");
    if (values.contains("record_segment_seconds"))
        loaded.segmentSeconds = values.value("record_segment_seconds").toInt();
    if (values.contains("capture_interval_ms"))
        loaded.captureIntervalMs = values.value("capture_interval_ms").toInt();
    for (int channel = 0; channel < 4; ++channel) {
        const QString nameKey = QStringLiteral("channel_%1_name").arg(channel + 1);
        const QString sourceKey = QStringLiteral("channel_%1_source").arg(channel + 1);
        if (values.contains(nameKey)) loaded.channelNames[channel] = values.value(nameKey);
        if (values.contains(sourceKey)) loaded.channelSources[channel] = values.value(sourceKey);
        const QString canonical = QStringLiteral("channel.%1").arg(channel + 1);
        if (values.contains(canonical)) {
            const auto json = QJsonDocument::fromJson(values.value(canonical).toUtf8());
            const auto object = json.object();
            if (!json.isObject() || object.value("channelId").toInt() != channel + 1
                || (object.value("sourceType") != "camera" && object.value("sourceType") != "file")) {
                errorText = QString::fromUtf8("通道%1配置损坏，未覆盖当前配置").arg(channel + 1);
                return false;
            }
            loaded.channelNames[channel] = object.value("name").toString();
            loaded.channelSources[channel] = object.value("sourceType") == "camera"
                ? QStringLiteral("camera:%1").arg(object.value("cameraIndex").toInt(-1))
                : object.value("filePath").toString();
        }
    }
    // 实际运行工程的键优先；保存时在同一事务内同步兼容键。
    if (values.contains("record.directory")) loaded.videoDirectory = values.value("record.directory");
    if (values.contains("record.segment_seconds")) loaded.segmentSeconds = values.value("record.segment_seconds").toInt();
    if (!validate(loaded, errorText)) return false;
    config = loaded;
    config.videoDirectory = storagePath(config.videoDirectory);
    config.imageDirectory = storagePath(config.imageDirectory);
    return true;
}

// 验证配置和目录可写性，在同一事务中保存优先键与兼容键。
bool SystemConfigService::save(const SystemConfig &config, QString &errorText)
{
    if (!validate(config, errorText)
            || !directoryWritable(storagePath(config.videoDirectory), errorText)
            || !directoryWritable(storagePath(config.imageDirectory), errorText)) {
        return false;
    }
    QMap<QString, QString> values;
    values.insert("video_dir", storagePath(config.videoDirectory));
    values.insert("image_dir", storagePath(config.imageDirectory));
    values.insert("record_segment_seconds", QString::number(config.segmentSeconds));
    values.insert("capture_interval_ms", QString::number(config.captureIntervalMs));
    values.insert("record.directory", storagePath(config.videoDirectory));
    values.insert("record.segment_seconds", QString::number(config.segmentSeconds));
    for (int channel = 0; channel < 4; ++channel) {
        values.insert(QStringLiteral("channel_%1_name").arg(channel + 1),
                      config.channelNames.at(channel).trimmed());
        values.insert(QStringLiteral("channel_%1_source").arg(channel + 1),
                      config.channelSources.at(channel).trimmed());
        const QString source = config.channelSources.at(channel).trimmed();
        const bool camera = source.startsWith("camera:");
        QJsonObject object;
        object.insert("channelId", channel + 1);
        object.insert("name", config.channelNames.at(channel).trimmed());
        object.insert("sourceType", camera ? "camera" : "file");
        object.insert("cameraIndex", camera ? source.mid(7).toInt() : -1);
        object.insert("filePath", camera ? QString() : source);
        values.insert(QStringLiteral("channel.%1").arg(channel + 1),
                      QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::Compact)));
    }
    return model.saveAll(values, errorText);
}

// 只检查字段、范围和摄像头分配合法性；不打开摄像头或加载模型。
bool SystemConfigService::validate(const SystemConfig &config, QString &errorText)
{
    errorText.clear();
    if (config.videoDirectory.trimmed().isEmpty()
            || config.imageDirectory.trimmed().isEmpty()) {
        errorText = QString::fromUtf8("视频和图片目录不能为空");
        return false;
    }
    if (config.segmentSeconds < 10 || config.segmentSeconds > 3600) {
        errorText = QString::fromUtf8("录像分段时长必须在10至3600秒之间");
        return false;
    }
    if (config.captureIntervalMs < 10 || config.captureIntervalMs > 1000) {
        errorText = QString::fromUtf8("采集间隔必须在10至1000毫秒之间");
        return false;
    }
    if (config.channelNames.size() != 4 || config.channelSources.size() != 4) {
        errorText = QString::fromUtf8("必须提供4路通道名称和来源");
        return false;
    }
    QSet<int> cameras;
    for (int index = 0; index < 4; ++index) {
        if (config.channelNames.at(index).trimmed().isEmpty()) {
            errorText = QString::fromUtf8("通道%1名称不能为空").arg(index + 1);
            return false;
        }
        const QString source = config.channelSources.at(index).trimmed();
        if (source.startsWith("camera:")) {
            bool valid = false;
            const int camera = source.mid(7).toInt(&valid);
            if (!valid || camera < 0 || cameras.contains(camera)) {
                errorText = QString::fromUtf8("通道%1的摄像头编号无效或重复").arg(index + 1);
                return false;
            }
            cameras.insert(camera);
        }
    }
    return true;
}

// 必要时创建目录并写一个唯一探测文件，随后删除探测文件。
bool SystemConfigService::directoryWritable(const QString &path,
                                            QString &errorText)
{
    QDir directory(QDir::cleanPath(path));
    if (!directory.exists() && !directory.mkpath(".")) {
        errorText = QString::fromUtf8("无法创建目录：%1").arg(path);
        return false;
    }
    const QString probe = directory.filePath(
                ".write-probe-" + QUuid::createUuid().toString(QUuid::Id128));
    QFile file(probe);
    if (!file.open(QIODevice::WriteOnly)) {
        errorText = QString::fromUtf8("目录不可写：%1").arg(path);
        return false;
    }
    file.close();
    QFile::remove(probe);
    return true;
}
