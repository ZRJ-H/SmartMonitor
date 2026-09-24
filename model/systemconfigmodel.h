// 【阅读导引】
// 成组配置持久化：接收外部数据库句柄，提供整组读取和事务保存；不负责控件刷新。

#ifndef SYSTEMCONFIGMODEL_H
#define SYSTEMCONFIGMODEL_H

#include <QMap>
#include <QSqlDatabase>
#include <QString>

class SystemConfigModel
{
public:
    explicit SystemConfigModel(const QSqlDatabase &database);

    bool loadAll(QMap<QString, QString> &values, QString &errorText) const;
    bool saveAll(const QMap<QString, QString> &values, QString &errorText);

private:
    QSqlDatabase db;
};

#endif // SYSTEMCONFIGMODEL_H
