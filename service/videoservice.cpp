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
    errorText.clear();

    const int total_records = video_model.countVideos();
    if (total_records < 0) {
        errorText = video_model.lastErrorText();
        return false;
    }

    const int total_pages = qMax(1, (total_records + page_size - 1) / page_size);
    const int current_page = qBound(1, requestedPage, total_pages);
    const int offset = (current_page - 1) * page_size;

    QStringList names;
    if (!video_model.queryVideoFileNames(offset, page_size, names)) {
        errorText = video_model.lastErrorText();
        return false;
    }

    page.fileNames = names;
    page.currentPage = current_page;
    page.totalPages = total_pages;
    page.totalRecords = total_records;
    return true;
}

int VideoService::pageSize() const
{
    return page_size;
}
