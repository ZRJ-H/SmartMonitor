// 【阅读导引】
// 异步数据库备份：QProcess 运行 mysqldump，成功后把 .partial 改成 .sql。
// start 返回表示是否发起操作，最终结果由 finished 信号给出；备份不含媒体文件本体。

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
