// 【阅读导引】
// 日志业务：检查时间、分页和删除确认，再委托 Model；不负责弹出对话框。

#ifndef OPERATIONLOGSERVICE_H
#define OPERATIONLOGSERVICE_H

#include "../model/operationlogmodel.h"

class OperationLogService
{
public:
    explicit OperationLogService(const QSqlDatabase &database);

    bool record(int adminId, const QString &operationFunc,
                const QString &operationDesc, const QDateTime &operationTime,
                QString &errorText);
    bool query(const OperationLogFilter &filter, QList<OperationLogEntry> &entries,
               QString &errorText, int page = 1, int pageSize = 100) const;
    bool operationNames(QStringList &names, QString &errorText) const;
    bool deleteIds(const QList<qint64> &ids,bool confirmed,QString &errorText);
    bool queryEvents(int channelId,const QDateTime &from,const QDateTime &to,int page,int pageSize,
                     QList<EventLogEntry> &entries,QString &errorText) const;
    bool deleteRange(const QDateTime &from, const QDateTime &to,
                     bool confirmed, QString &errorText);

private:
    OperationLogModel model;
};

#endif // OPERATIONLOGSERVICE_H
