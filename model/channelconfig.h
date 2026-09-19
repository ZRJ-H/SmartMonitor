#ifndef CHANNELCONFIG_H
#define CHANNELCONFIG_H

#include <QString>

// 一个监控通道只能从摄像头或本地视频文件中选择一种数据源。
enum class ChannelSourceType
{
    Camera,
    File
};

// 单个监控通道的完整配置，在界面、数据库和采集线程之间传递。
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
