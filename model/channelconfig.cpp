// 实现 ChannelConfig 的值比较，供配置变更检测和单元测试使用。
#include "channelconfig.h"

bool ChannelConfig::operator==(const ChannelConfig &other) const
{
    return channelId == other.channelId
            && name == other.name
            && sourceType == other.sourceType
            && cameraIndex == other.cameraIndex
            && filePath == other.filePath;
}
