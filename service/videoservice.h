// 声明录像分页业务服务，将数据库记录转换为界面可直接使用的页信息。
#ifndef VIDEOSERVICE_H
#define VIDEOSERVICE_H

#include "../model/videomodel.h"

#include <QString>
#include <QStringList>

// 一页录像列表及其分页元数据。
struct VideoPage
{
    VideoPage()
        : currentPage(1), totalPages(1), totalRecords(0)
    {
    }

    QStringList fileNames; // 当前页的录像文件名。
    QList<VideoRecord> records;
    int currentPage;       // 经过边界修正后的实际页码，从 1 开始。
    int totalPages;        // 总页数；无记录时仍按 1 页展示。
    int totalRecords;      // 数据库中的录像总记录数。
};

// 录像查询业务层，负责页码边界和 offset 计算，不直接操作界面。
class VideoService
{
public:
    explicit VideoService(int pageSize = 5); // pageSize 小于 1 时按 1 处理。

    // 加载请求页；成功写入 page，失败写入 errorText 并返回 false。
    bool loadPage(int requestedPage, VideoPage &page, QString &errorText);
    bool loadPage(const VideoQuery &query, int requestedPage,
                  VideoPage &page, QString &errorText);
    bool saveVideo(const Video &video, QString &errorText);
    bool saveEvent(const QString &eventKey, int channelId,
                   const QDateTime &eventTime, const QString &videoPath,
                   double confidence, QString &errorText);
    // 返回构造时确定的每页记录数。
    int pageSize() const;

private:
    VideoModel video_model; // video 表的数据访问对象。
    int page_size;          // 每页最大记录数，始终大于 0。
};

#endif // VIDEOSERVICE_H
