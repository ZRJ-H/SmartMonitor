// 实现录像记录的总数统计和稳定排序分页查询，供 VideoService 组合分页信息。
#include "videomodel.h"
#include "../util/dbconn.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

namespace {
QString filterSql(const VideoQuery &filter)
{
    QStringList clauses;
    if (filter.startTime.isValid()) clauses << QStringLiteral("v.start_time >= :start_time");
    if (filter.endTime.isValid()) clauses << QStringLiteral("v.start_time <= :end_time");
    if (filter.channelId > 0) clauses << QStringLiteral("v.channel_id = :channel_id");
    return clauses.isEmpty() ? QString()
                             : QStringLiteral(" WHERE ") + clauses.join(QStringLiteral(" AND "));
}

void bindFilter(QSqlQuery &query, const VideoQuery &filter)
{
    if (filter.startTime.isValid()) query.bindValue(QStringLiteral(":start_time"), filter.startTime);
    if (filter.endTime.isValid()) query.bindValue(QStringLiteral(":end_time"), filter.endTime);
    if (filter.channelId > 0) query.bindValue(QStringLiteral(":channel_id"), filter.channelId);
}
}

VideoModel::VideoModel()
    : last_error_text()
{
}

int VideoModel::countVideos()
{
    last_error_text.clear();
    DbConn *connection = DbConn::getInstance();
    if (connection == nullptr || !connection->isOpen()) {
        last_error_text = connection == nullptr
                ? QStringLiteral("数据库连接尚未创建")
                : connection->lastErrorText();
        return -1;
    }

    QSqlQuery query(connection->database());
    if (!query.exec("SELECT COUNT(*) FROM video")) {
        last_error_text = query.lastError().text();
        return -1;
    }

    return query.next() ? query.value(0).toInt() : 0;
}

bool VideoModel::queryVideoFileNames(int offset,
                                     int limit,
                                     QStringList &fileNames)
{
    last_error_text.clear();
    fileNames.clear();

    if (offset < 0 || limit <= 0) {
        last_error_text = QStringLiteral("分页参数无效");
        return false;
    }

    DbConn *connection = DbConn::getInstance();
    if (connection == nullptr || !connection->isOpen()) {
        last_error_text = connection == nullptr
                ? QStringLiteral("数据库连接尚未创建")
                : connection->lastErrorText();
        return false;
    }

    // offset/limit 由 Service 计算为非负整数，这里不接收用户文本。
    const QString sql = QString(
        "SELECT video_name FROM video "
        "ORDER BY start_time DESC, id DESC "
        "LIMIT %1 OFFSET %2"
    ).arg(limit).arg(offset);

    QSqlQuery query(connection->database());
    if (!query.exec(sql)) {
        last_error_text = query.lastError().text();
        return false;
    }

    while (query.next()) {
        fileNames.append(query.value("video_name").toString());
    }
    return true;
}

QString VideoModel::lastErrorText() const
{
    return last_error_text;
}

int VideoModel::countVideos(const VideoQuery &filter)
{
    last_error_text.clear();
    DbConn *connection = DbConn::getInstance();
    if (!connection || !connection->isOpen()) {
        last_error_text = connection ? connection->lastErrorText()
                                     : QStringLiteral("数据库连接尚未创建");
        return -1;
    }
    QSqlQuery query(connection->database());
    query.prepare(QStringLiteral("SELECT COUNT(*) FROM video v") + filterSql(filter));
    bindFilter(query, filter);
    if (!query.exec() || !query.next()) {
        last_error_text = query.lastError().text();
        return -1;
    }
    return query.value(0).toInt();
}

bool VideoModel::queryVideos(const VideoQuery &filter, int offset, int limit,
                             QList<VideoRecord> &records)
{
    records.clear();
    last_error_text.clear();
    if (offset < 0 || limit <= 0) {
        last_error_text = QStringLiteral("分页参数无效");
        return false;
    }
    DbConn *connection = DbConn::getInstance();
    if (!connection || !connection->isOpen()) {
        last_error_text = connection ? connection->lastErrorText()
                                     : QStringLiteral("数据库连接尚未创建");
        return false;
    }
    QSqlQuery query(connection->database());
    if (!query.exec(QStringLiteral(
        "CREATE TABLE IF NOT EXISTS monitor_event("
        "id INT AUTO_INCREMENT PRIMARY KEY,event_key VARCHAR(80) NOT NULL UNIQUE,"
        "channel_id INT NOT NULL,event_time DATETIME NOT NULL,video_path VARCHAR(1024),"
        "confidence DOUBLE NOT NULL,create_time TIMESTAMP DEFAULT CURRENT_TIMESTAMP)"))) {
        last_error_text = query.lastError().text();
        return false;
    }
    query.prepare(QStringLiteral(
        "SELECT v.id,v.video_name,v.video_path,v.channel_id,v.start_time,v.end_time,v.file_size,"
        "EXISTS(SELECT 1 FROM monitor_event e WHERE e.video_path=v.video_path) AS event_video "
        "FROM video v") + filterSql(filter)
        + QStringLiteral(" ORDER BY v.start_time DESC,v.id DESC LIMIT %1 OFFSET %2")
          .arg(limit).arg(offset));
    bindFilter(query, filter);
    if (!query.exec()) {
        last_error_text = query.lastError().text();
        return false;
    }
    while (query.next()) {
        VideoRecord record;
        record.id = query.value(0).toInt();
        record.name = query.value(1).toString();
        record.path = query.value(2).toString();
        record.channelId = query.value(3).toInt();
        record.startTime = query.value(4).toDateTime();
        record.endTime = query.value(5).toDateTime();
        record.fileSize = query.value(6).toLongLong();
        record.eventVideo = query.value(7).toBool();
        records.append(record);
    }
    return true;
}

bool VideoModel::insertVideo(const Video &video, int *insertedId)
{
    last_error_text.clear();
    DbConn *connection = DbConn::getInstance();
    if (!connection || !connection->isOpen()) {
        last_error_text = connection ? connection->lastErrorText()
                                     : QStringLiteral("数据库连接尚未创建");
        return false;
    }
    QSqlQuery query(connection->database());
    query.prepare(QStringLiteral(
        "INSERT INTO video(video_name,video_path,channel_id,start_time,end_time,file_size) "
        "VALUES(:name,:path,:channel,:start,:end,:size)"));
    query.bindValue(QStringLiteral(":name"), video.getVideoName());
    query.bindValue(QStringLiteral(":path"), video.getVideoPath());
    query.bindValue(QStringLiteral(":channel"), video.getChannelId());
    query.bindValue(QStringLiteral(":start"), video.getStartTime());
    query.bindValue(QStringLiteral(":end"), video.getEndTime());
    query.bindValue(QStringLiteral(":size"), video.getFileSize());
    if (!query.exec()) {
        last_error_text = query.lastError().text();
        return false;
    }
    if (insertedId) *insertedId = query.lastInsertId().toInt();
    return true;
}

bool VideoModel::createEvent(const QString &eventKey, int channelId,
                             const QDateTime &eventTime,
                             const QString &videoPath, double confidence)
{
    last_error_text.clear();
    DbConn *connection = DbConn::getInstance();
    if (!connection || !connection->isOpen()) {
        last_error_text = connection ? connection->lastErrorText()
                                     : QStringLiteral("数据库连接尚未创建");
        return false;
    }
    QSqlQuery query(connection->database());
    if (!query.exec(QStringLiteral(
        "CREATE TABLE IF NOT EXISTS monitor_event("
        "id INT AUTO_INCREMENT PRIMARY KEY,event_key VARCHAR(80) NOT NULL UNIQUE,"
        "channel_id INT NOT NULL,event_time DATETIME NOT NULL,video_path VARCHAR(1024),"
        "confidence DOUBLE NOT NULL,create_time TIMESTAMP DEFAULT CURRENT_TIMESTAMP)"))) {
        last_error_text = query.lastError().text();
        return false;
    }
    query.prepare(QStringLiteral(
        "INSERT IGNORE INTO monitor_event(event_key,channel_id,event_time,video_path,confidence) "
        "VALUES(:key,:channel,:time,:path,:confidence)"));
    query.bindValue(QStringLiteral(":key"), eventKey);
    query.bindValue(QStringLiteral(":channel"), channelId);
    query.bindValue(QStringLiteral(":time"), eventTime);
    query.bindValue(QStringLiteral(":path"), videoPath);
    query.bindValue(QStringLiteral(":confidence"), confidence);
    if (!query.exec()) {
        last_error_text = query.lastError().text();
        return false;
    }
    return true;
}
