// 【阅读导引】
// 已封口媒体恢复：先原子落盘待办，再尝试入库；成功才删待办，失败可在以后重放。
// 恢复范围不包含尚未封口且没有清单的损坏视频；数据库句柄仍需在所属线程使用。

#ifndef MEDIARECOVERYSERVICE_H
#define MEDIARECOVERYSERVICE_H
#include "../model/video.h"
#include <QSqlDatabase>
#include <QImage>
#include <QJsonObject>
// 文件封口后先原子保存待办清单，再入库；数据库失败不丢失帧和关联信息。
class MediaRecoveryService {
public:
    MediaRecoveryService(const QSqlDatabase &db,const QString &queueDirectory,const QString &imageDirectory);
    bool submit(const Video &video,const QString &eventKey,double confidence,
                const QList<QImage> &frames,const QList<qint64> &offsets,bool complete,QString &error);
    // stage() has no SQL access and may run on the recording producer thread.
    bool stage(const Video &video,const QString &eventKey,double confidence,
               const QList<QImage> &frames,const QList<qint64> &offsets,bool complete,
               QString &manifest,QString &error);
    bool applyStaged(const QString &manifest,QString &error,const QList<QImage> *originalFrames=nullptr);
    int replay(QStringList &errors,int limit=20);
private:
    bool apply(const QJsonObject &object,QString &error, const QList<QImage> *originalFrames=nullptr);
    QSqlDatabase db;
    QString queueDirectory,imageDirectory;
};
#endif
