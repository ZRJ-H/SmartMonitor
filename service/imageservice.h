// 【阅读导引】
// 图片业务：协调图片文件与 SQL 记录的保存、查询、导出和删除。
// 事务只覆盖 SQL，文件失败需要显式补偿；遇到未知同名图片不能直接覆盖。

#ifndef IMAGESERVICE_H
#define IMAGESERVICE_H

#include "../model/imagemodel.h"

#include <QImage>
#include <QSqlDatabase>

class ImageService
{
public:
    ImageService(const QSqlDatabase &database, const QString &imageDirectory);

    bool saveEventImages(const QList<QImage> &frames, int channelId,
                         qint64 exceptionId, const QDateTime &capturedAt,
                         QList<FeatureImage> &saved, QString &errorText,
                         qint64 videoId = 0, const QList<qint64> &offsets = {});
    bool saveManualImage(const QImage &frame, int channelId,
                         const QDateTime &capturedAt, qint64 videoId,
                         qint64 videoOffsetMs, FeatureImage &saved,
                         QString &errorText);
    bool query(const ImageFilter &filter, int page, int pageSize,
               QList<FeatureImage> &images, QString &errorText) const;
    bool exportImages(const QList<qint64> &ids, const QString &targetDirectory,
                      QString &errorText) const;
    bool deleteImages(const QList<qint64> &ids, bool confirmed,
                      QString &errorText);

private:
    bool ensureDirectory(QString &errorText) const;
    bool saveOne(const QImage &frame, FeatureImage &image, QString &errorText);

    QSqlDatabase db;
    QString directory;
    ImageModel model;
};

#endif // IMAGESERVICE_H
