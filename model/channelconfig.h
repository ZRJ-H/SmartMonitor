// 【阅读导引】
// 通道值对象：用一组字段描述数据源，不负责打开摄像头或连接数据库。
// 通道 ID 为 1～4，界面/任务数组下标为 0～3，转换时注意减一或加一。

// 定义监控通道的数据源类型和可在界面、数据库、采集线程之间传递的配置对象。
#ifndef CHANNELCONFIG_H
#define CHANNELCONFIG_H

#include <QString>

// 一个监控通道只能从摄像头或本地视频文件中选择一种数据源。
enum class ChannelSourceType
{
    Camera, // 使用本机摄像头，cameraIndex 指定设备序号。
    File    // 使用本地视频文件，filePath 指定文件位置。
};

// 单个监控通道的完整配置，在界面、数据库和采集线程之间传递。
struct ChannelConfig
{
    int channelId = 0; // 通道编号，当前业务约定为 1～4。
    QString name;      // 显示在通道列表中的用户可读名称。
    ChannelSourceType sourceType = ChannelSourceType::File; // 当前选择的数据源类型。
    int cameraIndex = -1; // 摄像头设备序号；仅摄像头源使用，文件源时忽略。
    QString filePath;     // 本地视频路径；仅文件源使用，摄像头源时忽略。

    // 比较两个配置的全部持久化字段是否一致。
    bool operator==(const ChannelConfig &other) const;
};

#endif // CHANNELCONFIG_H
