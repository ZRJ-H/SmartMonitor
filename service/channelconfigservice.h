// 声明通道配置业务服务，连接默认值/校验策略与 sys_config 数据访问层。
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
    // 读取四个通道配置；缺失记录保留默认值，失败原因写入 errorText。
    bool load(QList<ChannelConfig> &configs, QString &errorText);
    // 校验并以同一数据库事务保存全部通道；任一失败即整体回滚。
    bool save(const QList<ChannelConfig> &configs, QString &errorText);

private:
    ConfigModel config_model_; // 实际读写 sys_config 表的数据访问对象。
};

#endif // CHANNELCONFIGSERVICE_H
