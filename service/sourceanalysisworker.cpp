#include "sourceanalysisworker.h"
#include "detectionworker.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QDir>
#include <QFileInfo>
#include <QFile>
#include <QSaveFile>
#include <QCryptographicHash>
#include <QJsonDocument>
#include <QJsonArray>
#include <QImage>
#include <QUuid>
#include <QElapsedTimer>
#include <QThread>
#include <opencv2/videoio.hpp>
#include <cmath>
#include <stdexcept>
namespace { QString digest(const QByteArray &data){return QString::fromLatin1(QCryptographicHash::hash(data,QCryptographicHash::Sha256).toHex());}
void require(bool ok,const QString &error){if(!ok)throw std::runtime_error(error.toStdString());} }
struct SourceAnalysisWorker::Job {
    QString backend;
    QString key,asset,path,model,status="running";
    qint64 size=0,modified=0,nextMs=0,sample=0,activeSample=-1,activeStart=0,lastPositive=0,previousPts=-1,decoded=0;
    cv::VideoCapture capture;DetectionWorker detector;double threshold=0.5;
};
SourceAnalysisWorker::SourceAnalysisWorker(QString directory,QObject *parent):QObject(parent),directory_(std::move(directory)){
    timer_.setParent(this);timer_.setSingleShot(true);connect(&timer_,&QTimer::timeout,this,&SourceAnalysisWorker::tick);
}
SourceAnalysisWorker::~SourceAnalysisWorker(){shutdown();}
QString SourceAnalysisWorker::hashFile(const QString &path){
    QFile f(path);require(f.open(QIODevice::ReadOnly),"无法读取文件："+path);
    QCryptographicHash hash(QCryptographicHash::Sha256);
    while(!f.atEnd()){if(cancelled_.load())throw std::runtime_error("分析已取消");auto bytes=f.read(1024*1024);require(!bytes.isEmpty()||f.atEnd(),f.errorString());hash.addData(bytes);}
    return QString::fromLatin1(hash.result().toHex());
}
void SourceAnalysisWorker::exec(const QString &sql,const QVariantList &args){
    QSqlQuery q(db_);require(q.prepare(sql),q.lastError().text());for(const auto &arg:args)q.addBindValue(arg);require(q.exec(),q.lastError().text());
}
bool SourceAnalysisWorker::openIndex(QString &error){
    if(db_.isOpen())return true;
    try{
        require(QDir().mkpath(directory_),"无法创建文件分析目录");
        if(connectionName_.isEmpty()){connectionName_="source-index-"+QUuid::createUuid().toString(QUuid::Id128);db_=QSqlDatabase::addDatabase("QSQLITE",connectionName_);db_.setDatabaseName(QDir(directory_).filePath("source-index.sqlite"));}
        require(db_.open(),db_.lastError().text());
        QSqlQuery version(db_);require(version.exec("PRAGMA user_version")&&version.next(),version.lastError().text());
        require(version.value(0).toInt()<=1,"文件索引版本高于本程序支持范围");version.finish();
        exec("PRAGMA foreign_keys=ON");exec("PRAGMA journal_mode=WAL");exec("PRAGMA busy_timeout=3000");
        exec("CREATE TABLE IF NOT EXISTS assets(asset TEXT PRIMARY KEY,path TEXT NOT NULL,size INTEGER NOT NULL,modified INTEGER NOT NULL,ownership TEXT NOT NULL DEFAULT 'external_source')");
        exec("CREATE TABLE IF NOT EXISTS analyses(analysis TEXT PRIMARY KEY,asset TEXT NOT NULL REFERENCES assets(asset),profile TEXT NOT NULL,status TEXT NOT NULL,next_ms INTEGER NOT NULL DEFAULT 0,sample INTEGER NOT NULL DEFAULT 0,active_sample INTEGER NOT NULL DEFAULT -1,active_start INTEGER NOT NULL DEFAULT 0,last_positive INTEGER NOT NULL DEFAULT 0,analyzed_at TEXT NOT NULL)");
        exec("CREATE TABLE IF NOT EXISTS channel_analysis(channel INTEGER PRIMARY KEY,analysis TEXT NOT NULL REFERENCES analyses(analysis))");
        exec("CREATE TABLE IF NOT EXISTS analysis_channels(channel INTEGER NOT NULL,analysis TEXT NOT NULL REFERENCES analyses(analysis),PRIMARY KEY(channel,analysis))");
        exec("INSERT OR IGNORE INTO analysis_channels SELECT channel,analysis FROM channel_analysis");
        exec("CREATE TABLE IF NOT EXISTS events(event_key TEXT PRIMARY KEY,analysis TEXT NOT NULL REFERENCES analyses(analysis),start_ms INTEGER NOT NULL,end_ms INTEGER NOT NULL,confidence REAL NOT NULL,image TEXT NOT NULL,status TEXT NOT NULL)");
        exec("CREATE INDEX IF NOT EXISTS event_analysis_time ON events(analysis,start_ms)");
        exec("CREATE TABLE IF NOT EXISTS samples(analysis TEXT NOT NULL REFERENCES analyses(analysis),sample INTEGER NOT NULL,pts INTEGER NOT NULL,boxes TEXT NOT NULL,PRIMARY KEY(analysis,sample))");
        exec("CREATE INDEX IF NOT EXISTS sample_pts ON samples(analysis,pts)");
        exec("PRAGMA user_version=1");
        return true;
    }catch(const std::exception &e){error=QString::fromUtf8(e.what());db_.close();return false;}
}
void SourceAnalysisWorker::configure(int channel,const QString &path,bool enabled,const QString &model,double confidence){
    if(cancelled_.load()||channel<1||channel>4)return;
    channels_.remove(channel);
    {QMutexLocker l(&positionMutex_);positions_.remove(channel);}
    if(!enabled){
        if(db_.isOpen()&&path.isEmpty()){try{exec("DELETE FROM channel_analysis WHERE channel=?",{channel});}catch(const std::exception &e){emit message(channel,QString::fromUtf8(e.what()));}}
        if(db_.isOpen())refresh();
        return;
    }
    QString error;
    if(!openIndex(error)){emit message(channel,"文件索引不可用："+error);return;}
    try{
        const QFileInfo before(path);require(before.isFile(),"原视频不存在："+path);
        QString asset=hashFile(path);const QFileInfo after(path);
        require(before.size()==after.size()&&before.lastModified()==after.lastModified(),"散列时源文件发生变化");
        const QString profile=hashFile(model)+"|"+DetectionWorker::analysisProfile()+"|"+QString::number(confidence,'g',17)+"|sample500-gap3000-one-image-v1";
        const QString key=digest((asset+"|"+profile).toUtf8());
        exec("INSERT INTO assets(asset,path,size,modified) VALUES(?,?,?,?) ON CONFLICT(asset) DO UPDATE SET path=excluded.path,size=excluded.size,modified=excluded.modified",{asset,after.absoluteFilePath(),after.size(),after.lastModified().toMSecsSinceEpoch()});
        exec("INSERT OR IGNORE INTO analyses(analysis,asset,profile,status,analyzed_at) VALUES(?,?,?,'running',?)",{key,asset,profile,QDateTime::currentDateTime().toString(Qt::ISODateWithMs)});
        exec("INSERT INTO channel_analysis(channel,analysis) VALUES(?,?) ON CONFLICT(channel) DO UPDATE SET analysis=excluded.analysis",{channel,key});
        exec("INSERT OR IGNORE INTO analysis_channels(channel,analysis) VALUES(?,?)",{channel,key});
        channels_[channel]=key;
        if(jobs_.contains(key)&&jobs_[key]->status=="failed")jobs_.remove(key);
        if(!jobs_.contains(key)){
            auto job=std::make_shared<Job>();job->key=key;job->asset=asset;job->path=after.absoluteFilePath();job->model=model;job->threshold=confidence;job->size=after.size();job->modified=after.lastModified().toMSecsSinceEpoch();
            QSqlQuery q(db_);q.prepare("SELECT status,next_ms,sample,active_sample,active_start,last_positive FROM analyses WHERE analysis=?");q.addBindValue(key);require(q.exec()&&q.next(),q.lastError().text());
            job->status=q.value(0).toString();job->nextMs=q.value(1).toLongLong();job->sample=q.value(2).toLongLong();job->activeSample=q.value(3).toLongLong();job->activeStart=q.value(4).toLongLong();job->lastPositive=q.value(5).toLongLong();
            if(job->status!="complete"){
                const QString configureError=job->detector.configure(true,model,confidence,gpu_enabled_?"auto":"cpu");require(configureError.isEmpty(),configureError);
                require(job->capture.open(job->path.toStdString(),cv::CAP_FFMPEG,std::vector<int>{cv::CAP_PROP_N_THREADS,2}),"无法打开原视频分析");
                job->status="running";job->backend=job->detector.backend();
                emit message(channel,"文件分析中（每 0.5 秒取样）："+job->detector.backend());
            }else emit message(channel,"文件分析已完成，复用事件与截图");
            jobs_[key]=job;
        }else emit message(channel,"复用同一原视频的分析任务");
        refresh();timer_.start(0);
    }catch(const std::exception &e){channels_.remove(channel);emit message(channel,"文件分析失败："+QString::fromUtf8(e.what()));}
}
void SourceAnalysisWorker::setGpuEnabled(bool enabled){
    if(cancelled_.load() || gpu_enabled_==enabled)return;
    gpu_enabled_=enabled;
    const QString mode=enabled?QStringLiteral("auto"):QStringLiteral("cpu");
    // A job can serve several channels. Switch once, between samples, without
    // recreating decoders, analyses, events or screenshots.
    for(auto &job:jobs_){
        if(job->status!="running")continue;
        const QString error=job->detector.configure(true,job->model,job->threshold,mode);
        job->backend=job->detector.backend();
        for(auto it=channels_.cbegin();it!=channels_.cend();++it)if(it.value()==job->key)
            emit message(it.key(),error.isEmpty()?"检测后端："+job->backend
                :"后端切换失败，保留原后端："+job->backend+"；"+error);
    }
}
void SourceAnalysisWorker::requestPosition(int channel,qint64 pts,quint64 generation){
    QMutexLocker l(&positionMutex_);positions_[channel]={pts,generation};
}
void SourceAnalysisWorker::publishBoxes(){
    QMap<int,Position> pending;{QMutexLocker l(&positionMutex_);pending.swap(positions_);}
    for(auto it=pending.cbegin();it!=pending.cend();++it){
        QList<QRect> boxes;QString key=channels_.value(it.key());
        if(!key.isEmpty()){
            QSqlQuery q(db_);q.prepare("SELECT boxes FROM samples WHERE analysis=? AND pts<=? AND pts>=? ORDER BY pts DESC LIMIT 1");
            q.addBindValue(key);q.addBindValue(it->pts);q.addBindValue(qMax<qint64>(0,it->pts-500));
            if(q.exec()&&q.next())for(auto v:QJsonDocument::fromJson(q.value(0).toByteArray()).array()){auto a=v.toArray();if(a.size()==4)boxes<<QRect(a[0].toInt(),a[1].toInt(),a[2].toInt(),a[3].toInt());}
        }
        emit boxesReady(it.key(),it->pts,it->generation,boxes);
    }
}
void SourceAnalysisWorker::tick(){
    if(cancelled_.load())return;
    publishBoxes();
    // Inactive channels do not leave decoders/model sessions alive indefinitely.
    const auto active=channels_.values();
    for(auto it=jobs_.begin();it!=jobs_.end();){if(!active.contains(it.key()))it=jobs_.erase(it);else ++it;}
    QList<std::shared_ptr<Job>> running;for(auto j:jobs_)if(j->status=="running")running<<j;
    if(!running.isEmpty()){
        auto job=running.at(nextJob_++%running.size());
        try{process(*job);}catch(const std::exception &e){db_.rollback();job->status="failed";job->capture.release();
            for(auto it=channels_.cbegin();it!=channels_.cend();++it)if(it.value()==job->key)emit message(it.key(),"文件分析未完成："+QString::fromUtf8(e.what()));}
    }
    timer_.start(running.isEmpty()?50:1);
}
void SourceAnalysisWorker::process(Job &j){
    QFileInfo f(j.path);require(f.exists()&&f.size()==j.size&&f.lastModified().toMSecsSinceEpoch()==j.modified,"原视频被替换，停止当前分析");
    cv::Mat frame;bool sampled=false;bool eof=false;qint64 pts=0;
    // Bounded decode work per timer turn; the worker can process stop/configuration.
    for(int n=0;n<12&&!cancelled_.load();++n){
        if(!j.capture.read(frame)){eof=true;break;}++j.decoded;
        const double ms=j.capture.get(cv::CAP_PROP_POS_MSEC);
        require(std::isfinite(ms)&&ms>=0,"解码器未提供有效源时间戳");pts=qRound64(ms);
        require(j.previousPts<0||pts>j.previousPts,"源时间戳未递增，不能可靠定位事件");j.previousPts=pts;
        if(pts>=j.nextMs){sampled=true;break;}
    }
    if(cancelled_.load())return;
    if(eof){
        const qint64 expected=qRound64(j.capture.get(cv::CAP_PROP_FRAME_COUNT));
        require(expected<=0||j.decoded>=expected,"视频未完整解码，保留检查点待重试");
        // Re-hash before completion so a modified source cannot receive a complete stamp.
        require(hashFile(j.path)==j.asset,"分析期间源视频内容改变");
        require(db_.transaction(),db_.lastError().text());
        exec("UPDATE events SET status='complete' WHERE analysis=?",{j.key});
        exec("UPDATE analyses SET status='complete',active_sample=-1 WHERE analysis=?",{j.key});
        require(db_.commit(),db_.lastError().text());j.status="complete";j.capture.release();j.detector.configure(false,{},0.5);
        QSqlQuery q(db_);q.prepare("SELECT COUNT(*) FROM events WHERE analysis=?");q.addBindValue(j.key);require(q.exec()&&q.next(),q.lastError().text());
        emit analysisFinished(j.key,q.value(0).toInt());for(auto it=channels_.cbegin();it!=channels_.cend();++it)if(it.value()==j.key)emit message(it.key(),"文件分析完成，循环播放复用结果；原视频未重新录制");refresh();return;
    }
    if(!sampled)return;
    QElapsedTimer clock;clock.start();const auto result=j.detector.detect(frame);const auto elapsed=clock.elapsed();require(result.error.isEmpty(),result.error);
    for(auto it=channels_.cbegin();it!=channels_.cend();++it)if(it.value()==j.key)emit inferenceCompleted(it.key(),elapsed);
    if(result.backend!=j.backend){j.backend=result.backend;for(auto it=channels_.cbegin();it!=channels_.cend();++it)if(it.value()==j.key)emit message(it.key(),"检测后端："+j.backend);}
    QJsonArray boxes;for(auto b:result.boxes)boxes.append(QJsonArray{b.x,b.y,b.width,b.height});
    // Work with locals: on failure, persisted checkpoint remains the source of truth.
    qint64 active=j.activeSample,start=j.activeStart,last=j.lastPositive;
    bool newEvent=false;
    require(db_.transaction(),db_.lastError().text());
    exec("INSERT OR REPLACE INTO samples(analysis,sample,pts,boxes) VALUES(?,?,?,?)",{j.key,j.sample,pts,QString::fromUtf8(QJsonDocument(boxes).toJson(QJsonDocument::Compact))});
    if(active>=0 && pts-last>3000){exec("UPDATE events SET status='complete' WHERE event_key=?",{digest((j.key+":"+QString::number(active)).toUtf8())});active=-1;}
    if(result.found){
        if(active<0){active=j.sample;start=pts;newEvent=true;}
        last=pts;const QString key=digest((j.key+":"+QString::number(active)).toUtf8());
        const QString image=QDir(directory_).filePath("screenshots/"+key+".jpg");
        if(newEvent){
            require(QDir().mkpath(QFileInfo(image).absolutePath()),"无法创建事件截图目录");
            QImage view(frame.data,frame.cols,frame.rows,qsizetype(frame.step),QImage::Format_BGR888);
            QSaveFile file(image);require(file.open(QIODevice::WriteOnly),file.errorString());require(view.save(&file,"JPG",90),"截图编码失败");require(file.commit(),file.errorString());
        }
        exec("INSERT INTO events(event_key,analysis,start_ms,end_ms,confidence,image,status) VALUES(?,?,?,?,?,?,'active') ON CONFLICT(event_key) DO UPDATE SET end_ms=excluded.end_ms,confidence=MAX(confidence,excluded.confidence)",{key,j.key,start,last,result.confidence,image});
    }
    const qint64 next=j.nextMs+500;
    exec("UPDATE analyses SET status='running',next_ms=?,sample=?,active_sample=?,active_start=?,last_positive=? WHERE analysis=?",{next,j.sample+1,active,start,last,j.key});
    require(db_.commit(),db_.lastError().text());j.nextMs=next;++j.sample;j.activeSample=active;j.activeStart=start;j.lastPositive=last;
    if(newEvent)refresh();
}
void SourceAnalysisWorker::listEvents(int page,int channel,bool history){history_=history;listPage_=qMax(0,page);listChannel_=channel;QString e;if(!openIndex(e)){emit message(0,e);return;}refresh();}
void SourceAnalysisWorker::refresh(){
    if(!db_.isOpen())return;
    const QString binding=history_?"analysis_channels":"channel_analysis";
    const QString selected="EXISTS(SELECT 1 FROM "+binding+" c WHERE c.analysis=e.analysis AND (?=0 OR c.channel=?))";
    QSqlQuery count(db_);count.prepare("SELECT COUNT(*) FROM events e WHERE "+selected);count.addBindValue(listChannel_);count.addBindValue(listChannel_);
    if(!count.exec()||!count.next()){emit message(0,count.lastError().text());return;}const int total=count.value(0).toInt();listPage_=qMin(listPage_,qMax(0,(total-1)/50));
    QSqlQuery q(db_);q.prepare("SELECT e.event_key,a.path,e.image,e.start_ms,e.end_ms,e.confidence,a.asset,e.analysis,n.analyzed_at,(SELECT GROUP_CONCAT(channel) FROM analysis_channels WHERE analysis=e.analysis) FROM events e JOIN analyses n ON n.analysis=e.analysis JOIN assets a ON a.asset=n.asset WHERE "+selected+" ORDER BY n.analyzed_at DESC,e.start_ms LIMIT 50 OFFSET ?");
    q.addBindValue(listChannel_);q.addBindValue(listChannel_);q.addBindValue(listPage_*50);
    if(!q.exec()){emit message(0,q.lastError().text());return;}
    QList<SourceEventRow> rows;while(q.next()){SourceEventRow r;r.key=q.value(0).toString();r.path=q.value(1).toString();r.image=q.value(2).toString();r.startMs=q.value(3).toLongLong();r.endMs=q.value(4).toLongLong();r.confidence=q.value(5).toDouble();r.asset=q.value(6).toString();r.analysis=q.value(7).toString();r.analyzedAt=q.value(8).toString();r.channels=q.value(9).toString();rows<<r;}
    emit eventsReady(rows,listPage_,total);
}
void SourceAnalysisWorker::relocate(const QString &asset,const QString &path){
    QString error;if(!openIndex(error)){emit message(0,error);return;}
    try{require(hashFile(path)==asset,"所选文件内容与原视频不一致");QFileInfo f(path);
        exec("UPDATE assets SET path=?,size=?,modified=? WHERE asset=?",{f.absoluteFilePath(),f.size(),f.lastModified().toMSecsSinceEpoch(),asset});
        for(auto job:jobs_)if(job->asset==asset){job->path=f.absoluteFilePath();job->size=f.size();job->modified=f.lastModified().toMSecsSinceEpoch();}
        refresh();emit message(0,"已重新定位原视频");
    }catch(const std::exception &e){emit message(0,QString::fromUtf8(e.what()));}
}
void SourceAnalysisWorker::shutdown(){
    timer_.stop();jobs_.clear();channels_.clear();
    if(db_.isValid()){db_.close();db_=QSqlDatabase();QSqlDatabase::removeDatabase(connectionName_);}connectionName_.clear();
}

void SourceAnalysisWorker::resolvePlayback(const SourceEventRow &row,quint64 request){
    QString error;
    try{require(hashFile(row.path)==row.asset,"原视频内容已变化，请重新定位相同内容的文件");}
    catch(const std::exception &e){error=QString::fromUtf8(e.what());}
    emit playbackResolved(row,request,error);
}
