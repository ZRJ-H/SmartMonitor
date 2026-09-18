#include "videomodel.h"
#include "../util/dbconn.h"

#include <QSqlError>
#include <QSqlQuery>

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
