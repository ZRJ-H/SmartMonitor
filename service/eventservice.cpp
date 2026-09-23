#include "eventservice.h"
#include "imageservice.h"
#include <QSqlQuery>
#include <QSqlError>
bool EventService::save(const QString &key,const Video &video,double confidence,
                       const QList<QImage> &frames,const QList<qint64> &offsets,bool complete,QString &error)
{
    error.clear();
    if(key.isEmpty()||video.getChannelId()<1||video.getChannelId()>4||video.getFileSize()<=0
       ||(complete&&frames.size()!=3)||offsets.size()!=frames.size()){
        error="事件媒体参数不完整";return false;
    }
    qint64 exceptionId=0,videoId=0;
    QSqlQuery lookup(db);
    lookup.prepare("SELECT l.exception_id,l.video_id,e.event_status FROM event_media_link l JOIN exception_log e ON e.id=l.exception_id WHERE l.event_key=:key");
    lookup.bindValue(":key",key);
    if(!lookup.exec()){error=lookup.lastError().text();return false;}
    if(lookup.next()){
        exceptionId=lookup.value(0).toLongLong();videoId=lookup.value(1).toLongLong();
        if(lookup.value(2).toString()=="complete")return true;
    } else {
        if(!db.transaction()){error=db.lastError().text();return false;}
        QSqlQuery q(db);
        auto execute=[&](){if(q.exec())return true;error=q.lastError().text();db.rollback();return false;};
        q.prepare("INSERT INTO exception_log(channel_id,event_type,event_time,event_end_time,confidence,event_status,event_desc,related_video_path) VALUES(:channel,'person',:start,:end,:confidence,'pending',:key,:path)");
        q.bindValue(":channel",video.getChannelId());q.bindValue(":start",video.getStartTime());q.bindValue(":end",video.getEndTime());
        q.bindValue(":confidence",confidence);q.bindValue(":key",key);q.bindValue(":path",video.getVideoPath());
        if(!execute())return false;
        exceptionId=q.lastInsertId().toLongLong();
        q.prepare("INSERT INTO video(video_name,video_path,channel_id,start_time,end_time,file_size,video_type,exception_id) VALUES(:name,:path,:channel,:start,:end,:size,'event',:exception)");
        q.bindValue(":name",video.getVideoName());q.bindValue(":path",video.getVideoPath());q.bindValue(":channel",video.getChannelId());
        q.bindValue(":start",video.getStartTime());q.bindValue(":end",video.getEndTime());q.bindValue(":size",video.getFileSize());q.bindValue(":exception",exceptionId);
        if(!execute())return false;
        videoId=q.lastInsertId().toLongLong();
        q.prepare("INSERT INTO monitor_event(event_key,channel_id,event_time,video_path,confidence) VALUES(:key,:channel,:time,:path,:confidence)");
        q.bindValue(":key",key);q.bindValue(":channel",video.getChannelId());q.bindValue(":time",video.getStartTime());
        q.bindValue(":path",video.getVideoPath());q.bindValue(":confidence",confidence);
        if(!execute())return false;
        q.prepare("INSERT INTO event_media_link(event_key,exception_id,video_id) VALUES(:key,:exception,:video)");
        q.bindValue(":key",key);q.bindValue(":exception",exceptionId);q.bindValue(":video",videoId);
        if(!execute())return false;
        if(!db.commit()){error=db.lastError().text();db.rollback();return false;}
    }
    bool imagesOk=true;
    if(frames.size()==3){
        QSqlQuery count(db);count.prepare("SELECT COUNT(*) FROM feature_image WHERE exception_id=:id");count.bindValue(":id",exceptionId);
        if(!count.exec()||!count.next()){error=count.lastError().text();return false;}
        if(count.value(0).toInt()!=3){
            ImageService images(db,directory);QList<FeatureImage> saved;
            imagesOk=images.saveEventImages(frames,video.getChannelId(),exceptionId,video.getStartTime(),saved,error,videoId,offsets);
        }
    }
    QSqlQuery status(db);
    status.prepare("UPDATE exception_log SET event_status=:status WHERE id=:id");
    status.bindValue(":status",imagesOk ? (complete ? "complete" : "interrupted") : "media_failed");
    status.bindValue(":id",exceptionId);
    if(!status.exec()){error=status.lastError().text();return false;}
    return imagesOk;
}
