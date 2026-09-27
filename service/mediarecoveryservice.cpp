// 【阅读导引】
// 已封口媒体恢复：先原子落盘待办，再尝试入库；成功才删待办，失败可在以后重放。
// 恢复范围不包含尚未封口且没有清单的损坏视频；数据库句柄仍需在所属线程使用。

#include "mediarecoveryservice.h"
#include "eventservice.h"
#include "videoservice.h"
#include "../model/videomodel.h"
#include <QSaveFile>
#include <QSqlError>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QBuffer>
#include <QJsonDocument>
#include <QJsonArray>
#include <QCryptographicHash>
#include <algorithm>
// 构造对象并初始化所需成员；冒号后的初始化列表先于函数体执行。
MediaRecoveryService::MediaRecoveryService(const QSqlDatabase &database,const QString &queue,const QString &images)
 :db(database),queueDirectory(queue),imageDirectory(images){}
// 把元数据和采样帧写入恢复 JSON；apply 成功且清单移除成功才返回 true。
bool MediaRecoveryService::submit(const Video &v,const QString &key,double confidence,const QList<QImage> &frames,const QList<qint64> &offsets,bool complete,QString &error){
    QString manifest;
    return stage(v,key,confidence,frames,offsets,complete,manifest,error) && applyStaged(manifest,error,&frames);
}
bool MediaRecoveryService::stage(const Video &v,const QString &key,double confidence,const QList<QImage> &frames,const QList<qint64> &offsets,bool complete,QString &manifest,QString &error){
    error.clear();
    if(!QDir().mkpath(queueDirectory)){error="无法创建媒体恢复目录";return false;}
    QJsonObject o{{"version",1},{"key",key},{"name",v.getVideoName()},{"path",v.getVideoPath()},{"channel",v.getChannelId()},
        {"start",v.getStartTime().toString(Qt::ISODateWithMs)},{"end",v.getEndTime().toString(Qt::ISODateWithMs)},
        {"size",QString::number(v.getFileSize())},{"confidence",confidence},{"complete",complete}};
    QJsonArray images,times;
    for(const auto &frame:frames){QByteArray bytes;QBuffer buffer(&bytes);buffer.open(QIODevice::WriteOnly);
        if(!frame.save(&buffer,"PNG")){error="恢复帧序列保存失败";return false;}
        images.append(QString::fromLatin1(bytes.toBase64()));}
    for(auto offset:offsets)times.append(QString::number(offset));
    o["frames"]=images;o["offsets"]=times;
    const QString name=QString::fromLatin1(QCryptographicHash::hash(v.getVideoPath().toUtf8(),QCryptographicHash::Sha256).toHex())+".json";
    // 先把待处理业务写到本地恢复队列；QSaveFile 在 commit 时替换目标，避免留下半份 JSON。
    const QString path=QDir(queueDirectory).filePath(name);QSaveFile file(path);
    if(!file.open(QIODevice::WriteOnly)){error=file.errorString();return false;}
    const auto bytes=QJsonDocument(o).toJson(QJsonDocument::Compact);
    if(file.write(bytes)!=bytes.size()||!file.commit()){error=file.errorString();return false;}
    manifest=path;
    return true;
}
// Apply an already durable manifest; the producer may supply immutable frames to avoid decoding again.
bool MediaRecoveryService::applyStaged(const QString &manifest,QString &error,const QList<QImage> *frames){
    QFile file(manifest);
    if(!file.open(QIODevice::ReadOnly)){error=file.errorString();return false;}
    QJsonParseError parse;const auto document=QJsonDocument::fromJson(file.readAll(),&parse);file.close();
    if(parse.error!=QJsonParseError::NoError||!document.isObject()){error="恢复清单损坏";return false;}
    if(!apply(document.object(),error,frames))return false;
    if(!QFile::remove(manifest)){error="媒体已入库，但恢复清单未能移除";return false;}
    return true;
}
// 还原一项待办并检查录像存在；普通段按路径去重，事件交由 EventService 继续保存。
bool MediaRecoveryService::apply(const QJsonObject &o,QString &error,const QList<QImage> *originalFrames){
    if(o.value("version").toInt()!=1){error="不支持的恢复清单版本";return false;}
    Video video(o["name"].toString(),o["path"].toString(),o["channel"].toInt(),
        QDateTime::fromString(o["start"].toString(),Qt::ISODateWithMs),QDateTime::fromString(o["end"].toString(),Qt::ISODateWithMs),o["size"].toString().toLongLong());
    if(!QFileInfo::exists(video.getVideoPath())){error="恢复录像文件不存在："+video.getVideoPath();return false;}
    if(!db.isOpen()&&!db.open()){error=db.lastError().text();return false;}
    if(o["key"].toString().isEmpty()){
        VideoModel model(db);VideoRecord existing;
        if(model.findByPath(video.getVideoPath(),existing))return true;
        VideoService service(db);return service.saveVideo(video,error);
    }
    QList<QImage> frames;QList<qint64> offsets;
    if(originalFrames) frames=*originalFrames;
    else for(const auto &value:o["frames"].toArray())frames<<QImage::fromData(QByteArray::fromBase64(value.toString().toLatin1()),"PNG");
    for(const auto &value:o["offsets"].toArray())offsets<<value.toString().toLongLong();
    EventService service(db,imageDirectory);
    return service.save(o["key"].toString(),video,o["confidence"].toDouble(),frames,offsets,o["complete"].toBool(),error);
}
// 从上次游标后轮转尝试最多 limit 项；返回成功数，错误逐项收集且保留待办。
int MediaRecoveryService::replay(QStringList &errors,int limit){
    errors.clear();int count=0,attempted=0;
    QDir directory(queueDirectory);
    if (limit < 1) return 0;
    QStringList names=directory.entryList({"*.json"},QDir::Files,QDir::Name);
    QFile cursor(directory.filePath(".cursor"));
    QString last;
    if(cursor.open(QIODevice::ReadOnly)){last=QString::fromUtf8(cursor.readAll());cursor.close();}
    const auto after=std::upper_bound(names.begin(),names.end(),last);
    std::rotate(names.begin(),after,names.end());
    for(const auto &name:names){
        if(attempted++>=limit)break;
        QSaveFile nextCursor(directory.filePath(".cursor"));
        if(nextCursor.open(QIODevice::WriteOnly)){nextCursor.write(name.toUtf8());nextCursor.commit();}
        QFile file(directory.filePath(name));QString error;
        if(!file.open(QIODevice::ReadOnly)){errors<<file.errorString();continue;}
        QJsonParseError parse;const auto doc=QJsonDocument::fromJson(file.readAll(),&parse);file.close();
        if(parse.error!=QJsonParseError::NoError||!doc.isObject()){errors<<"恢复清单损坏："+name;continue;}
        if(!apply(doc.object(),error)){errors<<error;continue;}
        if(!QFile::remove(file.fileName())){errors<<"恢复清单无法移除："+name;continue;}
        ++count;
    }
    return count;
}
