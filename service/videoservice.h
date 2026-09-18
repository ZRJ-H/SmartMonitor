#ifndef VIDEOSERVICE_H
#define VIDEOSERVICE_H

#include "../model/videomodel.h"

#include <QString>
#include <QStringList>

struct VideoPage
{
    VideoPage()
        : currentPage(1), totalPages(1), totalRecords(0)
    {
    }

    QStringList fileNames;
    int currentPage;
    int totalPages;
    int totalRecords;
};

class VideoService
{
public:
    explicit VideoService(int pageSize = 5);

    bool loadPage(int requestedPage, VideoPage &page, QString &errorText);
    int pageSize() const;

private:
    VideoModel video_model;
    int page_size;
};

#endif // VIDEOSERVICE_H
