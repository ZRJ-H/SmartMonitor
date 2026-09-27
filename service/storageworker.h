#ifndef STORAGEWORKER_H
#define STORAGEWORKER_H
#include "../model/video.h"
#include <QThread>
#include <QSqlDatabase>
#include <QMutex>
#include <QWaitCondition>
#include <QQueue>
#include <QImage>
struct StorageConnection {
    QString driver,name,host,user,password,options;int port=-1;
    static StorageConnection from(const QSqlDatabase &db){return {db.driverName(),db.databaseName(),db.hostName(),db.userName(),db.password(),db.connectOptions(),db.port()};}
};
// A bounded producer queue. Only recording workers may submit media; the GUI
// uses the nonblocking requestReplay(). Each SQL connection belongs to run().
class StorageWorker:public QThread {
    Q_OBJECT
public:
    explicit StorageWorker(StorageConnection connection,QObject *parent=nullptr);
    ~StorageWorker() override;
    void setDirectories(const QString &queue,const QString &images);
    void submit(const Video &video,const QString &key={},double confidence=0,
                const QList<QImage> &frames={},const QList<qint64> &offsets={},bool complete=true);
    void requestReplay();
    void finish();
signals:
    void saved(const QString &key,bool complete);
    void warning(const QString &message);
protected:
    void run() override;
private:
    struct Job{Video video;QString key,queue,images,manifest;double confidence;QList<QImage> frames;QList<qint64> offsets;bool complete;};
    StorageConnection connection_;QMutex mutex_,stagingMutex_;QWaitCondition wake_,space_;
    QQueue<Job> jobs_;QString queue_,images_;bool stopping_=false,replay_=false;
};
#endif
