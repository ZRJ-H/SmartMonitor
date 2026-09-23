#include "operationlogservice.h"

OperationLogService::OperationLogService(const QSqlDatabase &database)
    : model(database)
{
}

bool OperationLogService::record(int adminId, const QString &operationFunc,
                                 const QString &operationDesc,
                                 const QDateTime &operationTime,
                                 QString &errorText)
{
    errorText.clear();
    if (adminId < 0 || operationFunc.trimmed().isEmpty() || !operationTime.isValid()) {
        errorText = QString::fromUtf8("日志参数无效");
        return false;
    }
    OperationLogEntry entry;
    entry.adminId = adminId;
    entry.operationTime = operationTime;
    entry.operationFunc = operationFunc.trimmed();
    entry.operationDesc = operationDesc.trimmed();
    return model.insert(entry, errorText);
}

bool OperationLogService::query(const OperationLogFilter &filter,
                                QList<OperationLogEntry> &entries,
                                QString &errorText, int page, int pageSize) const
{
    if (filter.from.isValid() && filter.to.isValid() && filter.from > filter.to) {
        errorText = QString::fromUtf8("开始时间不能晚于结束时间");
        return false;
    }
    return model.query(filter, entries, errorText, page, pageSize);
}

bool OperationLogService::deleteRange(const QDateTime &from, const QDateTime &to,
                                      bool confirmed, QString &errorText)
{
    errorText.clear();
    if (!confirmed) {
        errorText = QString::fromUtf8("删除日志前必须二次确认");
        return false;
    }
    if (!from.isValid() || !to.isValid() || from > to) {
        errorText = QString::fromUtf8("日志删除时间范围无效");
        return false;
    }
    return model.removeRange(from, to, errorText);
}

bool OperationLogService::deleteIds(const QList<qint64> &ids,bool confirmed,QString &errorText)
{
    errorText.clear();
    if(!confirmed||ids.isEmpty()){errorText="选择日志并确认后才能删除";return false;}
    for(auto id:ids)if(id<=0){errorText="日志编号无效";return false;}
    return model.removeIds(ids,errorText);
}
bool OperationLogService::queryEvents(int channelId,const QDateTime &from,const QDateTime &to,int page,int pageSize,
                                      QList<EventLogEntry> &entries,QString &errorText) const
{
    errorText.clear();
    if(channelId<0||channelId>4||page<1||pageSize<1||(from.isValid()&&to.isValid()&&from>to)){
        errorText="事件查询条件无效";return false;
    }
    return model.queryEvents(channelId,from,to,page,pageSize,entries,errorText);
}
