// 【阅读导引】
// 事件保存用例：以 eventKey 去重，事务建立事件/录像/关联，再保存图片并更新完成状态。
// SQL 事务不能回滚磁盘文件；失败重试依赖 MediaRecoveryService 保留的清单。

#ifndef EVENTSERVICE_H
#define EVENTSERVICE_H
#include "../model/video.h"
#include <QSqlDatabase>
#include <QImage>
// 元数据先作为 pending 提交，图片成功后才转 complete；失败可按 eventKey 重试。
class EventService {
public:
    EventService(const QSqlDatabase &db,const QString &imageDirectory):db(db),directory(imageDirectory){}
    bool save(const QString &eventKey,const Video &video,double confidence,
              const QList<QImage> &frames,const QList<qint64> &offsets,bool complete,QString &error);
private:
    QSqlDatabase db;
    QString directory;
};
#endif
