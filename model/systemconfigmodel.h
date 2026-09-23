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
