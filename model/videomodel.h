// 【阅读导引】
// 录像数据库访问：组合筛选、统计和分页，按 ID/路径查找并保存录像。
// 可注入测试数据库；默认使用 DbConn，复制数据库句柄不等于建立新连接。

// 声明 video 表的数据访问对象，为录像回放页提供总数统计和分页文件名查询。
#ifndef VIDEOMODEL_H
#define VIDEOMODEL_H

#include <QString>
#include <QSqlDatabase>
#include <QStringList>
#include <QDateTime>
#include <QList>
#include "video.h"

struct VideoQuery
{
    QDateTime startTime;
    QDateTime endTime;
    int channelId = 0;
    int eventType = -1; // -1 全部，0 普通，1 事件
};

struct VideoRecord
{
    int id = 0;
    QString name;
    QString path;
    int channelId = 0;
    QDateTime startTime;
    QDateTime endTime;
    qint64 fileSize = 0;
    bool eventVideo = false;
};

class VideoModel
{
public:
    VideoModel();
    explicit VideoModel(const QSqlDatabase &database);
    bool findById(qint64 id, VideoRecord &record);
    bool findByPath(const QString &path, VideoRecord &record);

    // 统计录像记录总数；失败返回 -1，并可通过 lastErrorText() 获取原因。
    int countVideos();
    // 从 offset 开始最多读取 limit 个录像文件名；查询成功返回 true。
    bool queryVideoFileNames(int offset, int limit, QStringList &fileNames);
    int countVideos(const VideoQuery &query);
    bool queryVideos(const VideoQuery &query, int offset, int limit,
                     QList<VideoRecord> &records);
    bool insertVideo(const Video &video, int *insertedId = nullptr);
    bool createEvent(const QString &eventKey, int channelId,
                     const QDateTime &eventTime, const QString &videoPath,
                     double confidence);
    // 返回最近一次数据库操作失败时的错误文本。
    QString lastErrorText() const;

private:
    QSqlDatabase database() const;
    QSqlDatabase injectedDatabase;
    bool injected = false;
    QString last_error_text; // 保存最近一次统计或分页查询失败的原因。
};

#endif // VIDEOMODEL_H
