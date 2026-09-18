#ifndef VIDEOMODEL_H
#define VIDEOMODEL_H

#include <QString>
#include <QStringList>

class VideoModel
{
public:
    VideoModel();

    int countVideos();
    bool queryVideoFileNames(int offset, int limit, QStringList &fileNames);
    QString lastErrorText() const;

private:
    QString last_error_text;
};

#endif // VIDEOMODEL_H
