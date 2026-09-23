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
    int replay(QStringList &errors,int limit=20);
private:
    bool apply(const QJsonObject &object,QString &error);
    QSqlDatabase db;
    QString queueDirectory,imageDirectory;
};
#endif
