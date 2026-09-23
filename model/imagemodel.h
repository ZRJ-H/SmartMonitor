#ifndef IMAGEMODEL_H
#define IMAGEMODEL_H

#include "featureimage.h"

#include <QList>
#include <QSqlDatabase>

class ImageModel
{
public:
    explicit ImageModel(const QSqlDatabase &database);

    bool insert(FeatureImage &image, QString &errorText);
    bool query(const ImageFilter &filter, int page, int pageSize,
               QList<FeatureImage> &images, QString &errorText) const;
    bool findByIds(const QList<qint64> &ids, QList<FeatureImage> &images,
                   QString &errorText) const;
    bool countForException(qint64 exceptionId, int &count,
                           QString &errorText) const;
    bool removeByIds(const QList<qint64> &ids, QString &errorText);

private:
    static FeatureImage fromQuery(const QSqlQuery &query);
    QSqlDatabase db;
};

#endif // IMAGEMODEL_H
