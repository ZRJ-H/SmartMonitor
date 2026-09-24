// 【阅读导引】
// 通道值对象：用一组字段描述数据源，不负责打开摄像头或连接数据库。
// 通道 ID 为 1～4，界面/任务数组下标为 0～3，转换时注意减一或加一。

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
