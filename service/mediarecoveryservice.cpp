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
MediaRecoveryService::MediaRecoveryService(const QSqlDatabase &database,const QString &queue,const QString &images)
 :db(database),queueDirectory(queue),imageDirectory(images){}
bool MediaRecoveryService::submit(const Video &v,const QString &key,double confidence,const QList<QImage> &frames,const QList<qint64> &offsets,bool complete,QString &error){
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
    const QString path=QDir(queueDirectory).filePath(name);QSaveFile file(path);
    if(!file.open(QIODevice::WriteOnly)){error=file.errorString();return false;}
    const auto bytes=QJsonDocument(o).toJson(QJsonDocument::Compact);
    if(file.write(bytes)!=bytes.size()||!file.commit()){error=file.errorString();return false;}
    if(!apply(o,error))return false;
    if(!QFile::remove(path)){error="媒体已入库，但恢复清单未能移除";return false;}
    return true;
}
bool MediaRecoveryService::apply(const QJsonObject &o,QString &error){
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
    for(const auto &value:o["frames"].toArray())frames<<QImage::fromData(QByteArray::fromBase64(value.toString().toLatin1()),"PNG");
    for(const auto &value:o["offsets"].toArray())offsets<<value.toString().toLongLong();
    EventService service(db,imageDirectory);
    return service.save(o["key"].toString(),video,o["confidence"].toDouble(),frames,offsets,o["complete"].toBool(),error);
}
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
