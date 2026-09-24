// 【阅读导引】
// 录像用例：校验请求、修正页码、组织 VideoPage，并向界面传递明确错误。

// 实现录像总页数、页码夹取和数据库分页偏移量计算。
#include "videoservice.h"

#include <QtGlobal>

// 构造对象并初始化所需成员；冒号后的初始化列表先于函数体执行。
VideoService::VideoService(int pageSize)
    : video_model(), page_size(qMax(1, pageSize))
{
}

// 构造对象并初始化所需成员；冒号后的初始化列表先于函数体执行。
VideoService::VideoService(const QSqlDatabase &db,int pageSize)
    : video_model(db),page_size(qMax(1,pageSize)) {}
// 校验 ID 后查找录像，把 Model 错误转给调用者。
bool VideoService::findById(qint64 id,VideoRecord &record,QString &errorText) {
    errorText.clear();
    if(video_model.findById(id,record)) return true;
    errorText=video_model.lastErrorText();return false;
}
// 统计总数、向上取整计算页数并夹取请求页；完整成功后更新 VideoPage。
bool VideoService::loadPage(int requestedPage,
                            VideoPage &page,
                            QString &errorText)
{
    return loadPage(VideoQuery(), requestedPage, page, errorText);
}

// 统计总数、向上取整计算页数并夹取请求页；完整成功后更新 VideoPage。
bool VideoService::loadPage(const VideoQuery &filter, int requestedPage,
                            VideoPage &page, QString &errorText)
{
    errorText.clear();

    if ((filter.startTime.isValid() && filter.endTime.isValid() && filter.startTime > filter.endTime)
        || filter.channelId < 0 || filter.channelId > 4 || filter.eventType < -1 || filter.eventType > 1) {
        errorText = QStringLiteral("查询条件无效"); return false;
    }
    const int total_records = video_model.countVideos(filter);
    if (total_records < 0) {
        errorText = video_model.lastErrorText();
        return false;
    }

    const int total_pages = qMax(1, (total_records + page_size - 1) / page_size);
    const int current_page = qBound(1, requestedPage, total_pages);
    const int offset = (current_page - 1) * page_size;

    QList<VideoRecord> records;
    if (!video_model.queryVideos(filter, offset, page_size, records)) {
        errorText = video_model.lastErrorText();
        return false;
    }

    page.records = records;
    page.fileNames.clear();
    for (const VideoRecord &record : records) page.fileNames.append(record.name);
    page.currentPage = current_page;
    page.totalPages = total_pages;
    page.totalRecords = total_records;
    return true;
}

// 只接收路径和大小有效的元数据；不能据此替代视频实际解码验收。
bool VideoService::saveVideo(const Video &video, QString &errorText, int *insertedId)
{
    errorText.clear();
    if (video.getVideoPath().isEmpty() || video.getFileSize() <= 0) {
        errorText = QStringLiteral("录像文件信息无效");
        return false;
    }
    if (!video_model.insertVideo(video, insertedId)) {
        errorText = video_model.lastErrorText();
        return false;
    }
    return true;
}

// 把旧式事件索引交给 Model 保存；完整三图事件链另由 EventService 处理。
bool VideoService::saveEvent(const QString &eventKey, int channelId,
                             const QDateTime &eventTime,
                             const QString &videoPath, double confidence,
                             QString &errorText)
{
    errorText.clear();
    if (!video_model.createEvent(eventKey, channelId, eventTime, videoPath,
                                 confidence)) {
        errorText = video_model.lastErrorText();
        return false;
    }
    return true;
}

// 返回每页最大条数，构造时已经确保大于零。
int VideoService::pageSize() const
{
    return page_size;
}
