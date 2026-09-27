// 【阅读导引】
// 日志业务：检查时间、分页和删除确认，再委托 Model；不负责弹出对话框。

#include "operationlogservice.h"

// 构造对象并初始化所需成员；冒号后的初始化列表先于函数体执行。
OperationLogService::OperationLogService(const QSqlDatabase &database)
    : model(database)
{
}

// 验证日志字段，adminId 为 0 表示系统动作，构造记录交给 Model。
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

// 先拒绝倒置时间范围，再查询操作日志页。
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

// 要求明确确认且时间范围有效；此操作按时间删除，不叠加界面其他筛选。
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

// 要求确认、非空选择和正整数 ID，然后只删所选操作日志。
bool OperationLogService::deleteIds(const QList<qint64> &ids,bool confirmed,QString &errorText)
{
    errorText.clear();
    if(!confirmed||ids.isEmpty()){errorText="选择日志并确认后才能删除";return false;}
    for(auto id:ids)if(id<=0){errorText="日志编号无效";return false;}
    return model.removeIds(ids,errorText);
}
// 校验通道、页码和时间范围，再查询异常事件与关联录像 ID。
bool OperationLogService::queryEvents(int channelId,const QDateTime &from,const QDateTime &to,int page,int pageSize,
                                      QList<EventLogEntry> &entries,QString &errorText) const
{
    errorText.clear();
    if(channelId<0||channelId>4||page<1||pageSize<1||(from.isValid()&&to.isValid()&&from>to)){
        errorText="事件查询条件无效";return false;
    }
    return model.queryEvents(channelId,from,to,page,pageSize,entries,errorText);
}

bool OperationLogService::operationNames(QStringList &names, QString &errorText) const
{
    return model.operationNames(names,errorText);
}
