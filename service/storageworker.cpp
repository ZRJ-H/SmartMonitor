#include "storageworker.h"
#include "mediarecoveryservice.h"
#include "operationlogservice.h"
#include <QSqlError>
#include <QUuid>
StorageWorker::StorageWorker(StorageConnection c,QObject *parent):QThread(parent),connection_(std::move(c)){}
StorageWorker::~StorageWorker(){finish();wait();}
void StorageWorker::setDirectories(const QString &q,const QString &i){QMutexLocker l(&mutex_);queue_=q;images_=i;}
void StorageWorker::submit(const Video &v,const QString &k,double c,const QList<QImage> &f,const QList<qint64> &o,bool done){
    QString queue,images;
    {QMutexLocker lock(&mutex_);queue=queue_;images=images_;}
    // Durable before enqueue: a process crash or shutdown cannot lose an accepted event.
    // This executes on RecordTask's thread, never on the GUI thread.
    QMutexLocker stagingLock(&stagingMutex_);
    QString manifest,error;
    MediaRecoveryService staging(QSqlDatabase(),queue,images);
    if(!staging.stage(v,k,c,f,o,done,manifest,error)){emit warning("媒体恢复清单写入失败，录像仍保留："+error);return;}
    QMutexLocker lock(&mutex_);
    while(jobs_.size()>=8 && !stopping_)space_.wait(&mutex_);
    if(stopping_){emit warning("存储已停止，待办已落盘，下次启动重试");return;}
    jobs_.enqueue({v,k,queue,images,manifest,c,f,o,done});wake_.wakeOne();
}
void StorageWorker::requestReplay(){QMutexLocker l(&mutex_);if(!stopping_){replay_=true;wake_.wakeOne();}}
void StorageWorker::finish(){QMutexLocker l(&mutex_);stopping_=true;wake_.wakeAll();space_.wakeAll();}
void StorageWorker::run(){
    const QString name="storage-"+QUuid::createUuid().toString(QUuid::Id128);
    {
        auto db=QSqlDatabase::addDatabase(connection_.driver,name);
        db.setDatabaseName(connection_.name);db.setHostName(connection_.host);db.setPort(connection_.port);
        db.setUserName(connection_.user);db.setPassword(connection_.password);
        QString options=connection_.options;
        if(connection_.driver=="QMYSQL")options+=";MYSQL_OPT_CONNECT_TIMEOUT=3;MYSQL_OPT_READ_TIMEOUT=3;MYSQL_OPT_WRITE_TIMEOUT=3";
        db.setConnectOptions(options);
        // submit() writes a recovery manifest even when opening SQL fails.
        db.open();
        while(true){
            Job job;bool replay=false,hasJob=false;QString queue,images;
            {
                QMutexLocker lock(&mutex_);
                while(jobs_.isEmpty()&&!replay_&&!stopping_)wake_.wait(&mutex_);
                if(!jobs_.isEmpty()){job=jobs_.dequeue();hasJob=true;space_.wakeOne();}
                else if(stopping_)break;
                else {replay=replay_;replay_=false;queue=queue_;images=images_;}
            }
            QString error;
            if(hasJob){
                MediaRecoveryService service(db,job.queue,job.images);
                if(!service.applyStaged(job.manifest,error,&job.frames))emit warning("媒体保存待重试："+error);
                else {if(!job.key.isEmpty()){OperationLogService audit(db);QString auditError;
                    if(!audit.record(0,"智能事件",job.key+(job.complete?" 已完成":" 提前结束"),QDateTime::currentDateTime(),auditError))emit warning("事件已保存，审计失败："+auditError);}
                    emit saved(job.key,job.complete);}
            } else if(replay){
                // Do not replay a manifest between its durable write and enqueue.
                // Never wait here: a producer may be waiting for this thread to drain.
                if(stagingMutex_.tryLock()){
                    bool queued;{QMutexLocker lock(&mutex_);queued=!jobs_.isEmpty();}
                    if(queued){stagingMutex_.unlock();continue;}
                    MediaRecoveryService service(db,queue,images);QStringList errors;
                    service.replay(errors,4);stagingMutex_.unlock();
                    if(!errors.isEmpty())emit warning("媒体恢复待处理："+errors.first());
                } // Busy producer will enqueue; older pending records retry next minute.
            }
        }
        db.close();
    }
    QSqlDatabase::removeDatabase(name);
}
