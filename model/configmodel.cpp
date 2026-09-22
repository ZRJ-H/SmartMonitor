// 实现 sys_config 表的参数化查询和“存在则更新、不存在则插入”写入逻辑。
#include "configmodel.h"
#include "../util/dbconn.h"

#include <QSqlError>
#include <QSqlQuery>

bool ConfigModel::findValue(const QString &key,
                            QString &value,
                            bool &found)
{
    // 先用返回值区分查询成功/失败；仅查询成功时，found 才表示是否找到记录。
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

    // 使用占位符绑定参数，避免手工拼接 SQL。
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

    // 同一个 key 已存在时直接更新，使保存操作可以重复执行。
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
