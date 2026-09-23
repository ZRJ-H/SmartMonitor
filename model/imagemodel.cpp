#include "imagemodel.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QStringList>

namespace {
QString imageColumns()
{
    return QStringLiteral("id,image_name,image_path,channel_id,capture_time,"
                          "capture_source,exception_id,video_id,video_offset_ms");
}

QString idPlaceholders(int count)
{
    QStringList values;
    for (int index = 0; index < count; ++index) {
        values << QStringLiteral(":id%1").arg(index);
    }
    return values.join(',');
}

void bindIds(QSqlQuery &query, const QList<qint64> &ids)
{
    for (int index = 0; index < ids.size(); ++index) {
        query.bindValue(QStringLiteral(":id%1").arg(index), ids.at(index));
    }
}
}

ImageModel::ImageModel(const QSqlDatabase &database)
    : db(database)
{
}

bool ImageModel::insert(FeatureImage &image, QString &errorText)
{
    errorText.clear();
    QSqlQuery query(db);
    query.prepare("INSERT INTO feature_image "
                  "(image_name,image_path,channel_id,capture_time,capture_source,"
                  "exception_id,video_id,video_offset_ms) "
                  "VALUES (:name,:path,:channel,:time,:source,:exception,:video,:offset)");
    query.bindValue(":name", image.imageName);
    query.bindValue(":path", image.imagePath);
    query.bindValue(":channel", image.channelId);
    query.bindValue(":time", image.captureTime);
    query.bindValue(":source", image.captureSource);
    query.bindValue(":exception", image.exceptionId > 0 ? QVariant(image.exceptionId) : QVariant());
    query.bindValue(":video", image.videoId > 0 ? QVariant(image.videoId) : QVariant());
    query.bindValue(":offset", image.videoId > 0 ? QVariant(image.videoOffsetMs) : QVariant());
    if (!query.exec()) {
        errorText = query.lastError().text();
        return false;
    }
    image.id = query.lastInsertId().toLongLong();
    return true;
}

bool ImageModel::query(const ImageFilter &filter, int page, int pageSize,
                       QList<FeatureImage> &images, QString &errorText) const
{
    images.clear();
    errorText.clear();
    if (page < 1 || pageSize < 1) {
        errorText = QString::fromUtf8("分页参数无效");
        return false;
    }

    QStringList conditions;
    if (filter.from.isValid()) conditions << "capture_time >= :from";
    if (filter.to.isValid()) conditions << "capture_time <= :to";
    if (filter.channelId > 0) conditions << "channel_id = :channel";
    if (!filter.captureSource.isEmpty()) conditions << "capture_source = :source";
    if (filter.exceptionId > 0) conditions << "exception_id = :exception";

    QString sql = QStringLiteral("SELECT %1 FROM feature_image").arg(imageColumns());
    if (!conditions.isEmpty()) sql += " WHERE " + conditions.join(" AND ");
    sql += " ORDER BY capture_time DESC,id DESC LIMIT :limit OFFSET :offset";

    QSqlQuery query(db);
    query.prepare(sql);
    if (filter.from.isValid()) query.bindValue(":from", filter.from);
    if (filter.to.isValid()) query.bindValue(":to", filter.to);
    if (filter.channelId > 0) query.bindValue(":channel", filter.channelId);
    if (!filter.captureSource.isEmpty()) query.bindValue(":source", filter.captureSource);
    if (filter.exceptionId > 0) query.bindValue(":exception", filter.exceptionId);
    query.bindValue(":limit", pageSize);
    query.bindValue(":offset", (page - 1) * pageSize);
    if (!query.exec()) {
        errorText = query.lastError().text();
        return false;
    }
    while (query.next()) images << fromQuery(query);
    return true;
}

bool ImageModel::findByIds(const QList<qint64> &ids, QList<FeatureImage> &images,
                           QString &errorText) const
{
    images.clear();
    errorText.clear();
    if (ids.isEmpty()) return true;
    QSqlQuery query(db);
    query.prepare(QStringLiteral("SELECT %1 FROM feature_image WHERE id IN (%2) ORDER BY id")
                  .arg(imageColumns(), idPlaceholders(ids.size())));
    bindIds(query, ids);
    if (!query.exec()) {
        errorText = query.lastError().text();
        return false;
    }
    while (query.next()) images << fromQuery(query);
    return true;
}

bool ImageModel::countForException(qint64 exceptionId, int &count,
                                   QString &errorText) const
{
    count = 0;
    errorText.clear();
    if (exceptionId <= 0) {
        errorText = QString::fromUtf8("异常事件编号无效");
        return false;
    }
    QSqlQuery query(db);
    query.prepare("SELECT COUNT(*) FROM feature_image WHERE exception_id=:exception");
    query.bindValue(":exception", exceptionId);
    if (!query.exec() || !query.next()) {
        errorText = query.lastError().text();
        return false;
    }
    count = query.value(0).toInt();
    return true;
}

bool ImageModel::removeByIds(const QList<qint64> &ids, QString &errorText)
{
    errorText.clear();
    if (ids.isEmpty()) return true;
    QSqlQuery query(db);
    query.prepare(QStringLiteral("DELETE FROM feature_image WHERE id IN (%1)")
                  .arg(idPlaceholders(ids.size())));
    bindIds(query, ids);
    if (!query.exec()) {
        errorText = query.lastError().text();
        return false;
    }
    return true;
}

FeatureImage ImageModel::fromQuery(const QSqlQuery &query)
{
    FeatureImage image;
    image.id = query.value("id").toLongLong();
    image.imageName = query.value("image_name").toString();
    image.imagePath = query.value("image_path").toString();
    image.channelId = query.value("channel_id").toInt();
    image.captureTime = query.value("capture_time").toDateTime();
    image.captureSource = query.value("capture_source").toString();
    image.exceptionId = query.value("exception_id").toLongLong();
    image.videoId = query.value("video_id").toLongLong();
    image.videoOffsetMs = query.value("video_offset_ms").toLongLong();
    return image;
}
