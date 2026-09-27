// 【阅读导引】
// 日志数据访问：操作日志和异常事件分别查询；管理员空值代表系统动作。
// 事件回放先查显式媒体关联，兼容旧记录时再按录像路径查找。

#include "operationlogmodel.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QStringList>

// 构造对象并初始化所需成员；冒号后的初始化列表先于函数体执行。
OperationLogModel::OperationLogModel(const QSqlDatabase &database)
    : db(database)
{
}

// 把系统动作的 adminId 0 写为 SQL NULL，成功后回填日志 ID。
bool OperationLogModel::insert(OperationLogEntry &entry, QString &errorText)
{
    errorText.clear();
    QSqlQuery query(db);
    query.prepare("INSERT INTO operation_log(admin_id,operation_time,operation_func,operation_desc) "
                  "VALUES(:admin,:time,:func,:desc)");
    query.bindValue(":admin", entry.adminId > 0 ? QVariant(entry.adminId) : QVariant());
    query.bindValue(":time", entry.operationTime);
    query.bindValue(":func", entry.operationFunc);
    query.bindValue(":desc", entry.operationDesc);
    if (!query.exec()) {
        errorText = query.lastError().text();
        return false;
    }
    entry.id = query.lastInsertId().toLongLong();
    return true;
}

// 组合可选条件，以时间和 ID 倒序分页，避免同时间记录顺序漂移。
bool OperationLogModel::query(const OperationLogFilter &filter,
                              QList<OperationLogEntry> &entries,
                              QString &errorText, int page, int pageSize) const
{
    entries.clear();
    errorText.clear();
    if (page < 1 || pageSize < 1) {
        errorText = QString::fromUtf8("分页参数无效");
        return false;
    }
    QStringList conditions;
    if (filter.adminId > 0) conditions << "admin_id=:admin";
    if (!filter.operationFunc.isEmpty()) conditions << "operation_func=:func";
    if (filter.from.isValid()) conditions << "operation_time>=:from";
    if (filter.to.isValid()) conditions << "operation_time<=:to";
    QString sql = "SELECT id,admin_id,operation_time,operation_func,operation_desc FROM operation_log";
    if (!conditions.isEmpty()) sql += " WHERE " + conditions.join(" AND ");
    sql += " ORDER BY operation_time DESC,id DESC LIMIT :limit OFFSET :offset";
    QSqlQuery query(db);
    query.prepare(sql);
    if (filter.adminId > 0) query.bindValue(":admin", filter.adminId);
    if (!filter.operationFunc.isEmpty()) query.bindValue(":func", filter.operationFunc);
    if (filter.from.isValid()) query.bindValue(":from", filter.from);
    if (filter.to.isValid()) query.bindValue(":to", filter.to);
    query.bindValue(":limit", pageSize);
    query.bindValue(":offset", (page - 1) * pageSize);
    if (!query.exec()) {
        errorText = query.lastError().text();
        return false;
    }
    while (query.next()) {
        OperationLogEntry entry;
        entry.id = query.value(0).toLongLong();
        entry.adminId = query.value(1).toInt();
        entry.operationTime = query.value(2).toDateTime();
        entry.operationFunc = query.value(3).toString();
        entry.operationDesc = query.value(4).toString();
        entries << entry;
    }
    return true;
}

// 删除包含两端边界的时间范围；确认和参数校验由服务层完成。
bool OperationLogModel::removeRange(const QDateTime &from, const QDateTime &to,
                                    QString &errorText)
{
    errorText.clear();
    QSqlQuery query(db);
    query.prepare("DELETE FROM operation_log WHERE operation_time>=:from AND operation_time<=:to");
    query.bindValue(":from", from);
    query.bindValue(":to", to);
    if (!query.exec()) {
        errorText = query.lastError().text();
        return false;
    }
    return true;
}

// 为 ID 列表逐个绑定 SQL 参数，不把数字列表当作用户 SQL 拼入。
bool OperationLogModel::removeIds(const QList<qint64> &ids,QString &errorText)
{
    QStringList placeholders;
    for(int i=0;i<ids.size();++i)placeholders<<QString(":id%1").arg(i);
    QSqlQuery query(db);query.prepare("DELETE FROM operation_log WHERE id IN ("+placeholders.join(",")+")");
    for(int i=0;i<ids.size();++i)query.bindValue(placeholders[i],ids[i]);
    if(!query.exec()){errorText=query.lastError().text();return false;}return true;
}
// 联接异常事件与媒体映射；无映射的旧记录尝试通过历史路径找到录像。
bool OperationLogModel::queryEvents(int channelId,const QDateTime &from,const QDateTime &to,int page,int pageSize,
                                    QList<EventLogEntry> &entries,QString &errorText) const
{
    QStringList filters;
    if(channelId>0)filters<<"e.channel_id=:channel";
    if(from.isValid())filters<<"e.event_time>=:from";
    if(to.isValid())filters<<"e.event_time<=:to";
    QString sql="SELECT e.id,e.channel_id,e.event_time,e.event_status,"
        "COALESCE(l.video_id,(SELECT v.id FROM video v WHERE v.video_path=e.related_video_path LIMIT 1),0) "
        "FROM exception_log e LEFT JOIN event_media_link l ON l.exception_id=e.id";
    if(!filters.isEmpty())sql+=" WHERE "+filters.join(" AND ");
    sql+=" ORDER BY e.event_time DESC,e.id DESC LIMIT :limit OFFSET :offset";
    QSqlQuery query(db);query.prepare(sql);
    if(channelId>0)query.bindValue(":channel",channelId);
    if(from.isValid())query.bindValue(":from",from);
    if(to.isValid())query.bindValue(":to",to);
    query.bindValue(":limit",pageSize);query.bindValue(":offset",(page-1)*pageSize);
    if(!query.exec()){errorText=query.lastError().text();return false;}
    QList<EventLogEntry> result;
    while(query.next()){
        EventLogEntry entry;entry.exceptionId=query.value(0).toLongLong();entry.channelId=query.value(1).toInt();
        entry.eventTime=query.value(2).toDateTime();entry.status=query.value(3).toString();entry.videoId=query.value(4).toLongLong();
        result<<entry;
    }
    entries=result;return true;
}

// 提供真实出现过的操作名称，界面无需让用户猜测可查询的文字。
bool OperationLogModel::operationNames(QStringList &names, QString &errorText) const
{
    names.clear();errorText.clear();
    QSqlQuery query(db);
    if (!query.exec("SELECT DISTINCT operation_func FROM operation_log WHERE operation_func <> '' ORDER BY operation_func")) {
        errorText=query.lastError().text();return false;
    }
    while(query.next()) names.append(query.value(0).toString());
    return true;
}
