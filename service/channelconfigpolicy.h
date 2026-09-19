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
    static QList<ChannelConfig> defaults();
    static bool validateAssignments(const QList<ChannelConfig> &configs,
                                    QString &errorText);
    static bool validateForStart(const ChannelConfig &config,
                                 QString &errorText);
    static QString encode(const ChannelConfig &config);
    static bool decode(const QString &json,
                       ChannelConfig &config,
                       QString &errorText);
};

#endif // CHANNELCONFIGPOLICY_H
