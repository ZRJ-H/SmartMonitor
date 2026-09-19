#include "configmodel.h"
#include "../util/dbconn.h"

#include <QSqlError>
#include <QSqlQuery>

bool ConfigModel::findValue(const QString &key,
                            QString &value,
                            bool &found)
{
    last_error_text_.clear();
    value.clear();
    found = false;

    DbConn *connection = DbConn::getInstance();
    if (connection == nullptr || !connection->isOpen()) {
        last_error_text_ = connection == nullptr
                ? QStringLiteral("数据库连接尚未创建")
                : connection->lastErrorText();
        return false;
    }

    QSqlQuery query(connection->database());
    query.prepare(QStringLiteral(
        "SELECT config_value FROM sys_config WHERE config_key=:key"));
    query.bindValue(QStringLiteral(":key"), key);
    if (!query.exec()) {
        last_error_text_ = query.lastError().text();
        return false;
    }
    if (query.next()) {
        value = query.value(0).toString();
        found = true;
    }
    return true;
}

bool ConfigModel::upsertValue(const QString &key, const QString &value)
{
    last_error_text_.clear();
    DbConn *connection = DbConn::getInstance();
    if (connection == nullptr || !connection->isOpen()) {
        last_error_text_ = connection == nullptr
                ? QStringLiteral("数据库连接尚未创建")
                : connection->lastErrorText();
        return false;
    }

    QSqlQuery query(connection->database());
    query.prepare(QStringLiteral(
        "INSERT INTO sys_config (config_key,config_value) "
        "VALUES (:key,:value) "
        "ON DUPLICATE KEY UPDATE config_value=VALUES(config_value)"));
    query.bindValue(QStringLiteral(":key"), key);
    query.bindValue(QStringLiteral(":value"), value);
    if (!query.exec()) {
        last_error_text_ = query.lastError().text();
        return false;
    }
    return true;
}

QString ConfigModel::lastErrorText() const
{
    return last_error_text_;
}
