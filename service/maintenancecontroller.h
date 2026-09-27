#ifndef MAINTENANCECONTROLLER_H
#define MAINTENANCECONTROLLER_H
#include <QObject>
class BackupService;
class QTimer;
class MaintenanceController : public QObject {
    Q_OBJECT
public:
    explicit MaintenanceController(QObject* parent = nullptr);
    void start();
    void stop();
public slots:
    void startBackup(int actorId);
signals:
    void message(const QString& message);
    void backupRunningChanged(bool running);
    void auditRequested(const QString& action, const QString& detail, int actorId);
    void recoveryRequested();

private:
    void backupIfDue();
    BackupService* backup_;
    QTimer *dailyBackup_, *recoveryTimer_;
    int actorId_ = 0;
    bool active_ = false;
};
#endif
