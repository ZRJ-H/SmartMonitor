// 实现录像总页数、页码夹取和数据库分页偏移量计算。
#include "videoservice.h"

#include <QtGlobal>

VideoService::VideoService(int pageSize)
    : video_model(), page_size(qMax(1, pageSize))
{
}

bool VideoService::loadPage(int requestedPage,
                            VideoPage &page,
                            QString &errorText)
{
    return loadPage(VideoQuery(), requestedPage, page, errorText);
}

bool VideoService::loadPage(const VideoQuery &filter, int requestedPage,
                            VideoPage &page, QString &errorText)
{
    errorText.clear();

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

bool VideoService::saveVideo(const Video &video, QString &errorText)
{
    errorText.clear();
    if (video.getVideoPath().isEmpty() || video.getFileSize() <= 0) {
        errorText = QStringLiteral("录像文件信息无效");
        return false;
    }
    if (!video_model.insertVideo(video)) {
        errorText = video_model.lastErrorText();
        return false;
    }
    return true;
}

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

int VideoService::pageSize() const
{
    return page_size;
}
