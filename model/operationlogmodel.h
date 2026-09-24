// 【阅读导引】
// 日志数据访问：操作日志和异常事件分别查询；管理员空值代表系统动作。
// 事件回放先查显式媒体关联，兼容旧记录时再按录像路径查找。

#ifndef OPERATIONLOGMODEL_H
#define OPERATIONLOGMODEL_H

#include <QDateTime>
#include <QList>
#include <QSqlDatabase>

struct OperationLogEntry
{
    qint64 id = 0;
    int adminId = 0;
    QDateTime operationTime;
    QString operationFunc;
    QString operationDesc;
};

struct EventLogEntry
{
    qint64 exceptionId=0;
    int channelId=0;
    QDateTime eventTime;
    QString status;
    qint64 videoId=0;
};

struct OperationLogFilter
{
    int adminId = 0;
    QString operationFunc;
    QDateTime from;
    QDateTime to;
};

class OperationLogModel
{
public:
    explicit OperationLogModel(const QSqlDatabase &database);

    bool insert(OperationLogEntry &entry, QString &errorText);
    bool query(const OperationLogFilter &filter, QList<OperationLogEntry> &entries,
               QString &errorText, int page, int pageSize) const;
    bool removeIds(const QList<qint64> &ids, QString &errorText);
    bool queryEvents(int channelId,const QDateTime &from,const QDateTime &to,int page,int pageSize,
                     QList<EventLogEntry> &entries,QString &errorText) const;
    bool removeRange(const QDateTime &from, const QDateTime &to,
                     QString &errorText);

private:
    QSqlDatabase db;
};

#endif // OPERATIONLOGMODEL_H
