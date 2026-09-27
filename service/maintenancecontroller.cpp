#include "maintenancecontroller.h"
#include "../util/dbconn.h"
#include "backupservice.h"
#include <QDateTime>
#include <QDir>
#include <QSettings>
#include <QStandardPaths>
#include <QTimer>
MaintenanceController::MaintenanceController(QObject* parent)
    : QObject(parent)
    , backup_(new BackupService(this))
    , dailyBackup_(new QTimer(this))
    , recoveryTimer_(new QTimer(this))
{
    setObjectName("maintenanceController");
    dailyBackup_->setObjectName("dailyBackupTimer");
    recoveryTimer_->setObjectName("mediaRecoveryTimer");
    connect(backup_, &BackupService::finished, this, [this](bool success, const QString& detail) {
        emit backupRunningChanged(false);
        emit message(
            success ? QString("数据库备份成功（不含视频和图片），保存位置：%1").arg(detail) : detail);
        if (success) {
            QSettings().setValue("backup/lastSuccess", QDateTime::currentDateTime());
            emit auditRequested("数据库备份", detail, actorId_);
        }
    });
    connect(dailyBackup_, &QTimer::timeout, this, &MaintenanceController::backupIfDue);
    connect(recoveryTimer_, &QTimer::timeout, this, [this] {
        if (active_ && qEnvironmentVariable("SMART_MONITOR_AUTOSTART", "1") != "0")
            emit recoveryRequested();
    });
}
void MaintenanceController::start()
{
    if (active_ || qEnvironmentVariable("SMART_MONITOR_AUTOSTART", "1") == "0")
        return;
    active_ = true;
    dailyBackup_->start(3600000);
    recoveryTimer_->start(60000);
    QTimer::singleShot(5000, this, [this] {
        if (active_)
            backupIfDue();
    });
    QTimer::singleShot(0, this, [this] {
        if (active_)
            emit recoveryRequested();
    });
}
void MaintenanceController::stop()
{
    active_ = false;
    dailyBackup_->stop();
    recoveryTimer_->stop();
}
void MaintenanceController::backupIfDue()
{
    if (!active_ || qEnvironmentVariable("SMART_MONITOR_AUTOSTART", "1") == "0")
        return;
    const auto last = QSettings().value("backup/lastSuccess").toDateTime();
    if (!last.isValid() || last.secsTo(QDateTime::currentDateTime()) >= 86400)
        startBackup(0);
}
void MaintenanceController::startBackup(int actorId)
{
    if (backup_->running())
        return;
    actorId_ = actorId;
    emit backupRunningChanged(true);
    const QString root = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    const QString executable = qEnvironmentVariable(
        "SMART_MONITOR_MYSQLDUMP", "C:/Program Files/MySQL/MySQL Server 5.7/bin/mysqldump.exe");
    backup_->start(DbConn::getInstance()->database(), QDir(root).filePath("backups"), executable);
}
