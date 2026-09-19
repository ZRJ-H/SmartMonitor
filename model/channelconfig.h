#ifndef CHANNELCONFIG_H
#define CHANNELCONFIG_H

#include <QString>

enum class ChannelSourceType
{
    Camera,
    File
};

struct ChannelConfig
{
    int channelId = 0;
    QString name;
    ChannelSourceType sourceType = ChannelSourceType::File;
    int cameraIndex = -1;
    QString filePath;

    bool operator==(const ChannelConfig &other) const;
};

#endif // CHANNELCONFIG_H
