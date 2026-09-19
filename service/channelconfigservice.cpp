#include "channelconfigservice.h"
#include "channelconfigpolicy.h"
#include "../util/dbconn.h"

#include <QSqlDatabase>
#include <QSqlError>

namespace {
QString configKey(int channelId)
{
    return QStringLiteral("channel.%1").arg(channelId);
}
}

bool ChannelConfigService::load(QList<ChannelConfig> &configs,
                                QString &errorText)
{
    errorText.clear();
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
