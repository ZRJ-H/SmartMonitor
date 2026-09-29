#ifndef RESOURCEMONITOR_H
#define RESOURCEMONITOR_H
#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <QString>
#include <QList>
#include <QPair>

namespace ResourceMetrics {
// -1 means unavailable, not zero utilization.
double cpuPercent(quint64 ticks, qint64 elapsedNs, unsigned processors);
double gpuPercent(const QList<QPair<QString,double>> &engines, quint64 pid);
QString percentText(double value);
}

// Samples only this process. Windows PDH queries never run on the GUI thread.
class ResourceMonitor final : public QThread {
    Q_OBJECT
public:
    explicit ResourceMonitor(QObject *parent=nullptr) : QThread(parent) {}
    ~ResourceMonitor() override;
    void stop();
signals:
    void usageReady(double cpu, double gpu, double memory);
protected:
    void run() override;
private:
    QMutex waitMutex_;
    QWaitCondition wake_;
};
#endif