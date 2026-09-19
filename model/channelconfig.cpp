#include "channelconfig.h"

bool ChannelConfig::operator==(const ChannelConfig &other) const
{
    return channelId == other.channelId
            && name == other.name
            && sourceType == other.sourceType
            && cameraIndex == other.cameraIndex
            && filePath == other.filePath;
}
