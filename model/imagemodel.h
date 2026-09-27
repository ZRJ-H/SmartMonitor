// 【阅读导引】
// 图片持久化：在 feature_image 表中查询/写入元数据，不读写 JPG 像素文件。
// 页码从 1 开始；时间加 ID 的稳定排序避免同时间记录在翻页时顺序不明确。

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
    // pageSize 为 -1 时返回全部匹配记录。
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
