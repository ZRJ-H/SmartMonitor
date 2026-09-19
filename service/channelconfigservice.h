#ifndef CHANNELCONFIGSERVICE_H
#define CHANNELCONFIGSERVICE_H

#include "../model/channelconfig.h"
#include "../model/configmodel.h"

#include <QList>
#include <QString>

class ChannelConfigService
{
public:
    bool load(QList<ChannelConfig> &configs, QString &errorText);
    bool save(const QList<ChannelConfig> &configs, QString &errorText);

private:
    ConfigModel config_model_;
};

#endif // CHANNELCONFIGSERVICE_H
