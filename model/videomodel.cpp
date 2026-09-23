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
VideoModel::VideoModel() = default;
VideoModel::VideoModel(const QSqlDatabase &db):injectedDatabase(db),injected(true) {}
QSqlDatabase VideoModel::database() const {
    return injected ? injectedDatabase : DbConn::getInstance()->database();
}
QString VideoModel::lastErrorText() const { return last_error_text; }
int VideoModel::countVideos() { return countVideos(VideoQuery()); }
int VideoModel::countVideos(const VideoQuery &f) {
    last_error_text.clear();
    const auto db=database();
    if(!db.isOpen()) {last_error_text="数据库未连接";return -1;}
    QSqlQuery q(db);q.prepare("SELECT COUNT(*) FROM video v"+filterSql(f));bindFilter(q,f);
    if(!q.exec()||!q.next()) {last_error_text=q.lastError().text();return -1;}
    return q.value(0).toInt();
}
bool VideoModel::queryVideoFileNames(int offset,int limit,QStringList &names) {
    QList<VideoRecord> records;
    if(!queryVideos(VideoQuery(),offset,limit,records)) return false;
    names.clear();for(const auto &v:records) names<<v.name;
    return true;
}
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
bool VideoModel::findById(qint64 id,VideoRecord &record) {
    last_error_text.clear();
    QSqlQuery q(database());q.prepare(columns()+" WHERE v.id=:id");q.bindValue(":id",id);
    if(!q.exec()) {last_error_text=q.lastError().text();return false;}
    if(!q.next()) {last_error_text="录像记录不存在";return false;}
    // 查找失败不能覆盖正在回放的记录。
    record=row(q);return true;
}
bool VideoModel::insertVideo(const Video &v,int *id) {
    last_error_text.clear();QSqlQuery q(database());
    q.prepare("INSERT INTO video(video_name,video_path,channel_id,start_time,end_time,file_size) VALUES(:name,:path,:channel,:start,:end,:size)");
    q.bindValue(":name",v.getVideoName());q.bindValue(":path",v.getVideoPath());q.bindValue(":channel",v.getChannelId());
    q.bindValue(":start",v.getStartTime());q.bindValue(":end",v.getEndTime());q.bindValue(":size",v.getFileSize());
    if(!q.exec()){last_error_text=q.lastError().text();return false;}
    if(id)*id=q.lastInsertId().toInt();
    return true;
}
bool VideoModel::createEvent(const QString &key,int channel,const QDateTime &time,const QString &path,double confidence) {
    last_error_text.clear();const auto db=database();QSqlQuery q(db);
    // 表由显式初始化/迁移创建，普通查询没有建表权限也能运行。
    const QString prefix=db.driverName()=="QSQLITE" ? "INSERT OR IGNORE" : "INSERT IGNORE";
    q.prepare(prefix+" INTO monitor_event(event_key,channel_id,event_time,video_path,confidence) VALUES(:key,:channel,:time,:path,:confidence)");
    q.bindValue(":key",key);q.bindValue(":channel",channel);q.bindValue(":time",time);q.bindValue(":path",path);q.bindValue(":confidence",confidence);
    if(!q.exec()){last_error_text=q.lastError().text();return false;}return true;
}

bool VideoModel::findByPath(const QString &path,VideoRecord &record) {
    last_error_text.clear();QSqlQuery query(database());
    query.prepare(columns()+" WHERE v.video_path=:path");query.bindValue(":path",path);
    if(!query.exec()){last_error_text=query.lastError().text();return false;}
    if(!query.next()){last_error_text="录像记录不存在";return false;}
    record=row(query);return true;
}
