// 【阅读导引】
// 通道配置用例：加载默认值与已存 JSON；保存四路配置时用同一事务避免部分更新。

// 实现通道配置的默认值合并、JSON 转换及数据库事务读写流程。
#include "channelconfigservice.h"
#include "channelconfigpolicy.h"
#include "../util/dbconn.h"

#include <QSqlDatabase>
#include <QSqlError>

namespace {
// 每个通道使用独立 key，便于以后单独读取或更新。
QString configKey(int channelId)
{
    return QStringLiteral("channel.%1").arg(channelId);
}
}

// 先创建四路默认配置，再逐项解码数据库 JSON，最后校验整组资源分配。
bool ChannelConfigService::load(QList<ChannelConfig> &configs,
                                QString &errorText)
{
    errorText.clear();
    // 数据库没有某个通道的记录时保留默认值，保证始终得到四个通道。
    configs = ChannelConfigPolicy::defaults();

    for (int i = 0; i < configs.size(); ++i) {
        QString value;
        bool found = false;
        if (!config_model_.findValue(configKey(i + 1), value, found)) {
            errorText = config_model_.lastErrorText();
            return false;
        }
        if (!found) {
            continue;
        }

        ChannelConfig decoded;
        if (!ChannelConfigPolicy::decode(value, decoded, errorText)) {
            errorText = QStringLiteral("通道 %1：%2").arg(i + 1).arg(errorText);
            return false;
        }
        if (decoded.channelId != i + 1) {
            errorText = QStringLiteral("通道 %1 的持久化编号不一致").arg(i + 1);
            return false;
        }
        configs[i] = decoded;
    }
    return ChannelConfigPolicy::validateAssignments(configs, errorText);
}

// 校验整组配置后开启事务；四个通道任一写入失败都回滚。
bool ChannelConfigService::save(const QList<ChannelConfig> &configs,
                                QString &errorText)
{
    errorText.clear();
    if (!ChannelConfigPolicy::validateAssignments(configs, errorText)) {
        return false;
    }

    DbConn *connection = DbConn::getInstance();
    if (connection == nullptr || !connection->isOpen()) {
        errorText = connection == nullptr
                ? QStringLiteral("数据库连接尚未创建")
                : connection->lastErrorText();
        return false;
    }

    // 四个通道必须作为一个整体保存，任意一个失败就全部回滚。
    QSqlDatabase database = connection->database();
    if (!database.transaction()) {
        errorText = database.lastError().text();
        return false;
    }

    for (const ChannelConfig &config : configs) {
        if (!config_model_.upsertValue(configKey(config.channelId),
                                       ChannelConfigPolicy::encode(config))) {
            database.rollback();
            errorText = config_model_.lastErrorText();
            return false;
        }
    }

    if (!database.commit()) {
        errorText = database.lastError().text();
        database.rollback();
        return false;
    }
    return true;
}
