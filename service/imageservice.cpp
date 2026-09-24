// 【阅读导引】
// 图片业务：协调图片文件与 SQL 记录的保存、查询、导出和删除。
// 事务只覆盖 SQL，文件失败需要显式补偿；遇到未知同名图片不能直接覆盖。

#include "imageservice.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSqlError>
#include <QTemporaryFile>
#include <QUuid>
#include <QBuffer>

// 构造对象并初始化所需成员；冒号后的初始化列表先于函数体执行。
ImageService::ImageService(const QSqlDatabase &database,
                           const QString &imageDirectory)
    : db(database),
      directory(QDir::cleanPath(imageDirectory)),
      model(database)
{
}

// 要求完整三图，在同一数据库事务中保存关联；失败补偿本次新建的文件。
bool ImageService::saveEventImages(const QList<QImage> &frames, int channelId,
                                   qint64 exceptionId, const QDateTime &capturedAt,
                                   QList<FeatureImage> &saved, QString &errorText,
                                   qint64 videoId, const QList<qint64> &offsets)
{
    saved.clear();
    errorText.clear();
    if (frames.size() != 3 || (videoId > 0 && offsets.size() != 3)) {
        errorText = QString::fromUtf8("异常事件必须正好3张图片");
        return false;
    }
    if (channelId < 1 || channelId > 4 || exceptionId <= 0 || !capturedAt.isValid()) {
        errorText = QString::fromUtf8("异常图片关联参数无效");
        return false;
    }
    if (!ensureDirectory(errorText)) return false;
    int existingCount = 0;
    if (!model.countForException(exceptionId, existingCount, errorText)) return false;
    if (existingCount > 0) {
        errorText = QString::fromUtf8("该异常事件已保存图片");
        return false;
    }
    const QString eventDirectory = QDir(directory).filePath(
                QStringLiteral("event-%1").arg(exceptionId));
    if (!QDir().mkpath(eventDirectory)) {
        errorText = QString::fromUtf8("无法创建异常事件图片目录");
        return false;
    }
    if (!db.transaction()) {
        if (errorText.isEmpty()) errorText = db.lastError().text();
        return false;
    }

    QList<QString> createdFiles;
    for (int index = 0; index < frames.size(); ++index) {
        FeatureImage image;
        image.imageName = QString::fromUtf8("异常_通道%1_%2_%3.jpg")
                .arg(channelId)
                .arg(capturedAt.toString("yyyyMMddHHmmss"))
                .arg(index + 1);
        image.imagePath = QDir(eventDirectory).filePath(image.imageName);
        image.channelId = channelId;
        image.captureTime = capturedAt;
        image.captureSource = "event";
        image.exceptionId = exceptionId;
        image.videoId = videoId;
        image.videoOffsetMs = offsets.isEmpty() ? 0 : offsets.at(index);
        image.captureTime = capturedAt.addMSecs(image.videoOffsetMs);
        const bool alreadyOnDisk = QFileInfo::exists(image.imagePath);
        if (!saveOne(frames.at(index), image, errorText)) {
            db.rollback();
            for (const QString &path : createdFiles) QFile::remove(path);
            saved.clear();
            return false;
        }
        if (!alreadyOnDisk) createdFiles << image.imagePath;
        saved << image;
    }
    if (!db.commit()) {
        errorText = db.lastError().text();
        db.rollback();
        for (const QString &path : createdFiles) QFile::remove(path);
        saved.clear();
        return false;
    }
    return true;
}

// 校验真实帧与关联参数，保存 JPG 和一条元数据；提交失败清理本次文件。
bool ImageService::saveManualImage(const QImage &frame, int channelId,
                                   const QDateTime &capturedAt, qint64 videoId,
                                   qint64 videoOffsetMs, FeatureImage &saved,
                                   QString &errorText)
{
    errorText.clear();
    if (frame.isNull() || channelId < 1 || channelId > 4 || !capturedAt.isValid()
            || videoId < 0 || videoOffsetMs < 0) {
        errorText = QString::fromUtf8("手动截图参数无效");
        return false;
    }
    if (!ensureDirectory(errorText)) return false;
    saved = FeatureImage();
    saved.imageName = QString::fromUtf8("手动_通道%1_%2.jpg")
            .arg(channelId)
            .arg(capturedAt.toString("yyyyMMddHHmmsszzz"));
    saved.imagePath = QDir(directory).filePath(saved.imageName);
    saved.channelId = channelId;
    saved.captureTime = capturedAt;
    saved.captureSource = "manual";
    saved.videoId = videoId;
    saved.videoOffsetMs = videoOffsetMs;
    if (!db.transaction()) {
        errorText = db.lastError().text();
        return false;
    }
    if (!saveOne(frame, saved, errorText)) {
        db.rollback();
        return false;
    }
    if (!db.commit()) {
        errorText = db.lastError().text();
        db.rollback();
        QFile::remove(saved.imagePath);
        return false;
    }
    return true;
}

// 校验起止时间再查询分页记录；图片本体由界面需要显示时加载。
bool ImageService::query(const ImageFilter &filter, int page, int pageSize,
                         QList<FeatureImage> &images, QString &errorText) const
{
    if (filter.from.isValid() && filter.to.isValid() && filter.from > filter.to) {
        errorText = QString::fromUtf8("开始时间不能晚于结束时间");
        return false;
    }
    return model.query(filter, page, pageSize, images, errorText);
}

// 先确认所选记录齐全再复制文件；不覆盖同名目标，失败清理本次已复制项。
bool ImageService::exportImages(const QList<qint64> &ids,
                                const QString &targetDirectory,
                                QString &errorText) const
{
    errorText.clear();
    if (ids.isEmpty()) {
        errorText = QString::fromUtf8("请先选择要导出的图片");
        return false;
    }
    QDir target(targetDirectory);
    if (!target.exists() && !target.mkpath(".")) {
        errorText = QString::fromUtf8("无法创建导出目录");
        return false;
    }
    QList<FeatureImage> images;
    if (!model.findByIds(ids, images, errorText)) return false;
    if (images.size() != ids.size()) {
        errorText = QString::fromUtf8("部分图片记录不存在");
        return false;
    }
    QList<QString> copied;
    for (const FeatureImage &image : images) {
        const QString destination = target.filePath(image.imageName);
        if (!QFileInfo::exists(image.imagePath) || QFileInfo::exists(destination)
                || !QFile::copy(image.imagePath, destination)) {
            for (const QString &path : copied) QFile::remove(path);
            errorText = QString::fromUtf8("导出图片失败：%1").arg(image.imageName);
            return false;
        }
        copied << destination;
    }
    return true;
}

// 要求确认后暂时改名锁定文件，再删记录并提交；SQL 失败尝试恢复原名。
bool ImageService::deleteImages(const QList<qint64> &ids, bool confirmed,
                                QString &errorText)
{
    errorText.clear();
    if (!confirmed) {
        errorText = QString::fromUtf8("删除图片前必须二次确认");
        return false;
    }
    if (ids.isEmpty()) {
        errorText = QString::fromUtf8("请先选择要删除的图片");
        return false;
    }
    QList<FeatureImage> images;
    if (!model.findByIds(ids, images, errorText)) return false;
    if (images.size() != ids.size()) {
        errorText = QString::fromUtf8("部分图片记录不存在");
        return false;
    }

    QList<QPair<QString, QString>> moved;
    for (const FeatureImage &image : images) {
        if (!QFileInfo::exists(image.imagePath)) continue;
        const QString temporary = image.imagePath + ".deleting-" + QUuid::createUuid().toString(QUuid::Id128);
        if (!QFile::rename(image.imagePath, temporary)) {
            for (const auto &pair : moved) QFile::rename(pair.second, pair.first);
            errorText = QString::fromUtf8("无法锁定待删除图片：%1").arg(image.imageName);
            return false;
        }
        moved << qMakePair(image.imagePath, temporary);
    }

    if (!db.transaction() || !model.removeByIds(ids, errorText) || !db.commit()) {
        if (errorText.isEmpty()) errorText = db.lastError().text();
        db.rollback();
        for (const auto &pair : moved) QFile::rename(pair.second, pair.first);
        return false;
    }
    for (const auto &pair : moved) QFile::remove(pair.second);
    return true;
}

// 检查图片根目录并按需创建，不代表之后每一次文件写入都不会失败。
bool ImageService::ensureDirectory(QString &errorText) const
{
    if (directory.isEmpty() || directory == ".") {
        errorText = QString::fromUtf8("图片目录不能为空");
        return false;
    }
    QDir dir(directory);
    if (!dir.exists() && !dir.mkpath(".")) {
        errorText = QString::fromUtf8("无法创建图片目录");
        return false;
    }
    return true;
}

// 先写临时 JPG 再改最终名；仅对字节相同的孤立事件图片接纳入库，未知同名文件拒绝。
bool ImageService::saveOne(const QImage &frame, FeatureImage &image,
                           QString &errorText)
{
    if (!frame.isNull() && image.captureSource == "event" && image.exceptionId > 0
            && QFileInfo::exists(image.imagePath)) {
        QByteArray expected; QBuffer buffer(&expected); buffer.open(QIODevice::WriteOnly);
        QFile existing(image.imagePath);
        if (frame.save(&buffer,"JPG",92) && existing.open(QIODevice::ReadOnly)
                && existing.readAll() == expected) {
            // The metadata transaction rolled back after an atomic file rename.
            // Adopt only byte-identical event media; never overwrite an unknown file.
            return model.insert(image,errorText);
        }
    }
    if (frame.isNull() || QFileInfo::exists(image.imagePath)) {
        errorText = QString::fromUtf8("图片文件保存失败：%1").arg(image.imageName);
        return false;
    }

    const QFileInfo destination(image.imagePath);
    QTemporaryFile temporary(QDir(destination.absolutePath()).filePath(".capture-XXXXXX.tmp"));
    temporary.setAutoRemove(true);
    if (!temporary.open() || !frame.save(&temporary, "JPG", 92)) {
        errorText = QString::fromUtf8("图片文件保存失败：%1").arg(image.imageName);
        return false;
    }
    temporary.close();
    if (!temporary.rename(image.imagePath)) {
        errorText = QString::fromUtf8("图片文件保存失败：%1").arg(image.imageName);
        return false;
    }
    temporary.setAutoRemove(false);
    if (!model.insert(image, errorText)) {
        QFile::remove(image.imagePath);
        return false;
    }
    return true;
}
