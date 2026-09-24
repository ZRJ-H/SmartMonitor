// 【阅读导引】
// 通道纯规则：生成默认配置、校验资源分配、转换 JSON；IO 启停由 MonitorTask 执行。

// 声明四路监控通道的默认配置、业务校验和 JSON 持久化格式。
#ifndef CHANNELCONFIGPOLICY_H
#define CHANNELCONFIGPOLICY_H

#include "../model/channelconfig.h"

#include <QList>
#include <QString>

// 集中管理通道默认值、业务校验以及 JSON 序列化规则。
// 将这些规则放在界面之外，便于复用和单元测试。
class ChannelConfigPolicy
{
public:
    // 创建四个通道的默认配置列表，顺序与界面下标一致。
    static QList<ChannelConfig> defaults();
    // 校验整组配置的数量、编号、名称和数据源分配；失败原因写入 errorText。
    static bool validateAssignments(const QList<ChannelConfig> &configs,
                                    QString &errorText);
    // 校验单个通道是否具备启动条件，如摄像头序号或文件路径是否有效。
    static bool validateForStart(const ChannelConfig &config,
                                 QString &errorText);
    // 将单个通道配置编码为数据库中保存的紧凑 JSON 字符串。
    static QString encode(const ChannelConfig &config);
    // 解析并校验 JSON；成功时写入 config，失败原因写入 errorText。
    static bool decode(const QString &json,
                       ChannelConfig &config,
                       QString &errorText);
};

#endif // CHANNELCONFIGPOLICY_H
