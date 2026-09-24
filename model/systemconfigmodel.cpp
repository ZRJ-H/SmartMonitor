// 【阅读导引】
// 成组配置持久化：接收外部数据库句柄，提供整组读取和事务保存；不负责控件刷新。

#include "systemconfigmodel.h"

#include <QSqlError>
#include <QSqlQuery>

// 构造对象并初始化所需成员；冒号后的初始化列表先于函数体执行。
SystemConfigModel::SystemConfigModel(const QSqlDatabase &database)
    : db(database)
{
}

// 把 sys_config 行转成键值映射，失败通过 errorText 返回。
bool SystemConfigModel::loadAll(QMap<QString, QString> &values,
                                QString &errorText) const
{
    values.clear();
    errorText.clear();
    QSqlQuery query(db);
    if (!query.exec("SELECT config_key,config_value FROM sys_config")) {
        errorText = query.lastError().text();
        return false;
    }
    while (query.next()) values.insert(query.value(0).toString(), query.value(1).toString());
    return true;
}

// 成组检查键是否存在并更新/插入；任一 SQL 或提交失败就回滚。
bool SystemConfigModel::saveAll(const QMap<QString, QString> &values,
                                QString &errorText)
{
    errorText.clear();
    if (!db.transaction()) {
        errorText = db.lastError().text();
        return false;
    }
    for (auto it = values.cbegin(); it != values.cend(); ++it) {
        QSqlQuery exists(db);
        exists.prepare("SELECT 1 FROM sys_config WHERE config_key=:key");
        exists.bindValue(":key", it.key());
        if (!exists.exec()) {
            errorText = exists.lastError().text();
            db.rollback();
            return false;
        }
        const bool update = exists.next();
        QSqlQuery write(db);
        if (update) {
            write.prepare("UPDATE sys_config SET config_value=:value WHERE config_key=:key");
        } else {
            write.prepare("INSERT INTO sys_config(config_key,config_value) VALUES(:key,:value)");
        }
        write.bindValue(":key", it.key());
        write.bindValue(":value", it.value());
        if (!write.exec()) {
            errorText = write.lastError().text();
            db.rollback();
            return false;
        }
    }
    if (!db.commit()) {
        errorText = db.lastError().text();
        db.rollback();
        return false;
    }
    return true;
}
