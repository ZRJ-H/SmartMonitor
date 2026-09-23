#ifndef BACKUPSERVICE_H
#define BACKUPSERVICE_H
#include <QObject>
#include <QProcess>
#include <QSqlDatabase>
class BackupService : public QObject {
    Q_OBJECT
public:
    explicit BackupService(QObject *parent=nullptr);
    bool start(const QSqlDatabase &database, const QString &directory, const QString &executable);
    bool running() const { return process.state() != QProcess::NotRunning; }
signals:
    void finished(bool success, const QString &detail);
private:
    QProcess process;
    QString pendingPath, finalPath;
    void finish(bool success, const QString &error);
};
#endif
