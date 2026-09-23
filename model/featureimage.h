#ifndef FEATUREIMAGE_H
#define FEATUREIMAGE_H

#include <QDateTime>
#include <QString>

struct FeatureImage
{
    qint64 id = 0;
    QString imageName;
    QString imagePath;
    int channelId = 0;
    QDateTime captureTime;
    QString captureSource;
    qint64 exceptionId = 0;
    qint64 videoId = 0;
    qint64 videoOffsetMs = 0;
};

struct ImageFilter
{
    QDateTime from;
    QDateTime to;
    int channelId = 0;
    QString captureSource;
    qint64 exceptionId = 0;
};

#endif // FEATUREIMAGE_H
