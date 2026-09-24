// 【阅读导引】
// 录像数据库访问：组合筛选、统计和分页，按 ID/路径查找并保存录像。
// 可注入测试数据库；默认使用 DbConn，复制数据库句柄不等于建立新连接。

#include "videomodel.h"
#include "../util/dbconn.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>

namespace {
const QString eventExists = "EXISTS(SELECT 1 FROM monitor_event e WHERE e.video_path=v.video_path)";
QString filterSql(const VideoQuery &filter) {
    QStringList clauses;
    if(filter.startTime.isValid()) clauses << "v.start_time>=:start";
    if(filter.endTime.isValid()) clauses << "v.start_time<=:end";
    if(filter.channelId>0) clauses << "v.channel_id=:channel";
    if(filter.eventType==1) clauses << eventExists;
    if(filter.eventType==0) clauses << "NOT " + eventExists;
    return clauses.isEmpty() ? QString() : " WHERE " + clauses.join(" AND ");
}
void bindFilter(QSqlQuery &q,const VideoQuery &f) {
    if(f.startTime.isValid()) q.bindValue(":start",f.startTime);
    if(f.endTime.isValid()) q.bindValue(":end",f.endTime);
    if(f.channelId>0) q.bindValue(":channel",f.channelId);
}
VideoRecord row(const QSqlQuery &q) {
    VideoRecord v;
    v.id=q.value(0).toInt();v.name=q.value(1).toString();v.path=q.value(2).toString();
    v.channelId=q.value(3).toInt();v.startTime=q.value(4).toDateTime();
    v.endTime=q.value(5).toDateTime();v.fileSize=q.value(6).toLongLong();v.eventVideo=q.value(7).toBool();
    return v;
}
QString columns() {
    return "SELECT v.id,v.video_name,v.video_path,v.channel_id,v.start_time,v.end_time,v.file_size,"
        + eventExists + " FROM video v";
}
}
// 构造对象并初始化所需成员；冒号后的初始化列表先于函数体执行。
VideoModel::VideoModel() = default;
// 构造对象并初始化所需成员；冒号后的初始化列表先于函数体执行。
VideoModel::VideoModel(const QSqlDatabase &db):injectedDatabase(db),injected(true) {}
// 优先返回构造时注入的数据库，否则使用应用单例；句柄副本共享同一连接。
QSqlDatabase VideoModel::database() const {
    return injected ? injectedDatabase : DbConn::getInstance()->database();
}
// 返回最近一次操作错误，空字符串不代表一定找到了记录。
QString VideoModel::lastErrorText() const { return last_error_text; }
// 统计满足条件的录像数量；失败返回 -1，不能把它当成空表。
int VideoModel::countVideos() { return countVideos(VideoQuery()); }
// 统计满足条件的录像数量；失败返回 -1，不能把它当成空表。
int VideoModel::countVideos(const VideoQuery &f) {
    last_error_text.clear();
    const auto db=database();
    if(!db.isOpen()) {last_error_text="数据库未连接";return -1;}
    QSqlQuery q(db);q.prepare("SELECT COUNT(*) FROM video v"+filterSql(f));bindFilter(q,f);
    if(!q.exec()||!q.next()) {last_error_text=q.lastError().text();return -1;}
    return q.value(0).toInt();
}
// 复用录像查询再提取名称，保留旧调用接口。
bool VideoModel::queryVideoFileNames(int offset,int limit,QStringList &names) {
    QList<VideoRecord> records;
    if(!queryVideos(VideoQuery(),offset,limit,records)) return false;
    names.clear();for(const auto &v:records) names<<v.name;
    return true;
}
// 以起点时间及 ID 倒序读取一页，先构造临时结果，成功才覆盖输出。
bool VideoModel::queryVideos(const VideoQuery &f,int offset,int limit,QList<VideoRecord> &records) {
    last_error_text.clear();
    if(offset<0||limit<1) {last_error_text="分页参数无效";return false;}
    const auto db=database();
    if(!db.isOpen()) {last_error_text="数据库未连接";return false;}
    QSqlQuery q(db);
    q.prepare(columns()+filterSql(f)+" ORDER BY v.start_time DESC,v.id DESC LIMIT :limit OFFSET :offset");
    bindFilter(q,f);q.bindValue(":limit",limit);q.bindValue(":offset",offset);
    if(!q.exec()) {last_error_text=q.lastError().text();return false;}
    QList<VideoRecord> result;while(q.next())result<<row(q);
    records=result;return true;
}
// 按主键查录像；找不到或 SQL 失败时不覆盖调用者当前记录。
bool VideoModel::findById(qint64 id,VideoRecord &record) {
    last_error_text.clear();
    QSqlQuery q(database());q.prepare(columns()+" WHERE v.id=:id");q.bindValue(":id",id);
    if(!q.exec()) {last_error_text=q.lastError().text();return false;}
    if(!q.next()) {last_error_text="录像记录不存在";return false;}
    // 查找失败不能覆盖正在回放的记录。
    record=row(q);return true;
}
// 保存已封口录像元数据；可选 insertedId 指针接收新 ID，传空则忽略。
bool VideoModel::insertVideo(const Video &v,int *id) {
    last_error_text.clear();QSqlQuery q(database());
    q.prepare("INSERT INTO video(video_name,video_path,channel_id,start_time,end_time,file_size) VALUES(:name,:path,:channel,:start,:end,:size)");
    q.bindValue(":name",v.getVideoName());q.bindValue(":path",v.getVideoPath());q.bindValue(":channel",v.getChannelId());
    q.bindValue(":start",v.getStartTime());q.bindValue(":end",v.getEndTime());q.bindValue(":size",v.getFileSize());
    if(!q.exec()){last_error_text=q.lastError().text();return false;}
    if(id)*id=q.lastInsertId().toInt();
    return true;
}
// 以唯一事件键执行忽略重复的兼容插入；表结构由显式迁移准备。
bool VideoModel::createEvent(const QString &key,int channel,const QDateTime &time,const QString &path,double confidence) {
    last_error_text.clear();const auto db=database();QSqlQuery q(db);
    // 表由显式初始化/迁移创建，普通查询没有建表权限也能运行。
    const QString prefix=db.driverName()=="QSQLITE" ? "INSERT OR IGNORE" : "INSERT IGNORE";
    q.prepare(prefix+" INTO monitor_event(event_key,channel_id,event_time,video_path,confidence) VALUES(:key,:channel,:time,:path,:confidence)");
    q.bindValue(":key",key);q.bindValue(":channel",channel);q.bindValue(":time",time);q.bindValue(":path",path);q.bindValue(":confidence",confidence);
    if(!q.exec()){last_error_text=q.lastError().text();return false;}return true;
}

// 按完整路径查已有录像，供恢复重试避免重复插入。
bool VideoModel::findByPath(const QString &path,VideoRecord &record) {
    last_error_text.clear();QSqlQuery query(database());
    query.prepare(columns()+" WHERE v.video_path=:path");query.bindValue(":path",path);
    if(!query.exec()){last_error_text=query.lastError().text();return false;}
    if(!query.next()){last_error_text="录像记录不存在";return false;}
    record=row(query);return true;
}
