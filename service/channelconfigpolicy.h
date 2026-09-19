#ifndef CHANNELCONFIGPOLICY_H
#define CHANNELCONFIGPOLICY_H

#include "../model/channelconfig.h"

#include <QList>
#include <QString>

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
