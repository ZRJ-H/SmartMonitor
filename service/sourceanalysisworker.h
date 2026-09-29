#ifndef SOURCEANALYSISWORKER_H
#define SOURCEANALYSISWORKER_H
#include <QObject>
#include <QSqlDatabase>
#include <QTimer>
#include <QMutex>
#include <QMap>
#include <QRect>
#include <memory>
#include <atomic>
#include "sourceeventrow.h"
// This object's entire decoder/index/model state lives on its worker thread.
// Only requestPosition()/cancel() are thread-safe cross-thread entry points.
class SourceAnalysisWorker:public QObject {
    Q_OBJECT
public:
    explicit SourceAnalysisWorker(QString directory,QObject *parent=nullptr);
    ~SourceAnalysisWorker() override;
    void requestPosition(int channel,qint64 pts,quint64 generation);
    void cancel(){cancelled_.store(true);}
public slots:
    void setGpuEnabled(bool enabled); // Worker-thread only; retains analysis checkpoints.
    void configure(int channel,const QString &path,bool enabled,const QString &model,double confidence);
    void listEvents(int page=0,int channel=0,bool history=false);
    void relocate(const QString &asset,const QString &path);
    void shutdown();
    void resolvePlayback(const SourceEventRow &row,quint64 request);
signals:
    void message(int channel,const QString &text);
    void inferenceCompleted(int channel,qint64 elapsedMs);
    void eventsReady(const QList<SourceEventRow> &events,int page,int total);
    void boxesReady(int channel,qint64 pts,quint64 generation,const QList<QRect> &boxes);
    void playbackResolved(const SourceEventRow &row,quint64 request,const QString &error);
    void analysisFinished(const QString &key,int eventCount);
private:
    bool gpu_enabled_ = false;
    struct Job;
    struct Position {qint64 pts=0;quint64 generation=0;};
    QString directory_,connectionName_;QSqlDatabase db_;QTimer timer_;
    QMap<QString,std::shared_ptr<Job>> jobs_;QMap<int,QString> channels_;int nextJob_=0;
    QMutex positionMutex_;QMap<int,Position> positions_;
    std::atomic_bool cancelled_{false};int listPage_=0,listChannel_=0;bool history_=false;
    bool openIndex(QString &error);
    void tick();void process(Job &job);void publishBoxes();
    QString hashFile(const QString &path);
    void exec(const QString &sql,const QVariantList &args={});
    void refresh();
};
#endif
