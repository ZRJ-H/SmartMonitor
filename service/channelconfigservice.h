#ifndef CHANNELCONFIGSERVICE_H
#define CHANNELCONFIGSERVICE_H

#include "../model/channelconfig.h"
#include "../model/configmodel.h"

#include <QList>
#include <QString>

// 通道配置业务层：组合默认值、校验规则和数据库读写。
class ChannelConfigService
{
public:
    bool load(QList<ChannelConfig> &configs, QString &errorText);
    bool save(const QList<ChannelConfig> &configs, QString &errorText);

private:
    ConfigModel config_model_;
};

#endif // CHANNELCONFIGSERVICE_H
