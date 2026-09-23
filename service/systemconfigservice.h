#ifndef SYSTEMCONFIGSERVICE_H
#define SYSTEMCONFIGSERVICE_H

#include "../model/systemconfigmodel.h"

#include <QStringList>

struct SystemConfig
{
    QString videoDirectory = "./records";
    QString imageDirectory = "./images";
    int segmentSeconds = 60;
    int captureIntervalMs = 40;
    QStringList channelNames = {"通道1", "通道2", "通道3", "通道4"};
    QStringList channelSources = {"camera:0", "", "", ""};
};

class SystemConfigService
{
public:
    explicit SystemConfigService(const QSqlDatabase &database);

    bool load(SystemConfig &config, QString &errorText) const;
    bool save(const SystemConfig &config, QString &errorText);
    static bool validate(const SystemConfig &config, QString &errorText);

private:
    static bool directoryWritable(const QString &path, QString &errorText);
    SystemConfigModel model;
};

#endif // SYSTEMCONFIGSERVICE_H
