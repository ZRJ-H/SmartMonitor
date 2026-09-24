// 【阅读导引】
// 异步数据库备份：QProcess 运行 mysqldump，成功后把 .partial 改成 .sql。
// start 返回表示是否发起操作，最终结果由 finished 信号给出；备份不含媒体文件本体。

#include "backupservice.h"
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcessEnvironment>
#include <QTimer>
#include <QUuid>
// 构造对象并初始化所需成员；冒号后的初始化列表先于函数体执行。
BackupService::BackupService(QObject *parent):QObject(parent) {
    connect(&process,&QProcess::finished,this,[this](int code,QProcess::ExitStatus status){
        finish(code==0 && status==QProcess::NormalExit, QString::fromLocal8Bit(process.readAllStandardError()));
    });
    connect(&process,&QProcess::errorOccurred,this,[this](QProcess::ProcessError error){
        if(error==QProcess::FailedToStart) finish(false,process.errorString());
    });
}
// 验证条件后异步发起 mysqldump；返回 true 仅表示已发起，完成必须等待 finished。
bool BackupService::start(const QSqlDatabase &db,const QString &directory,const QString &executable) {
    if(running()) return false;
    if(db.driverName()!="QMYSQL" || !QFileInfo::exists(executable) || !QDir().mkpath(directory)){
        emit finished(false,"备份需要 QMYSQL 数据库、可写目录及 mysqldump.exe"); return false;
    }
    const QString name=db.databaseName()+"-"+QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss")+"-"+QUuid::createUuid().toString(QUuid::Id128).left(8)+".sql";
    finalPath=QDir(directory).absoluteFilePath(name); pendingPath=finalPath+".partial";
    auto env=QProcessEnvironment::systemEnvironment();env.insert("MYSQL_PWD",db.password());
    process.setProcessEnvironment(env);
    process.start(executable,{"--host="+db.hostName(),"--user="+db.userName(),"--default-character-set=utf8mb4",
                             "--single-transaction","--routines","--events","--hex-blob",
                             "--result-file="+pendingPath,db.databaseName()});
    const QString attempt=pendingPath;
    QTimer::singleShot(120000,this,[this,attempt]{if(pendingPath==attempt&&running())process.kill();});
    return true;
}
// 只在正常退出、非空文件且改名成功时报告成功；失败清理当前 .partial。
void BackupService::finish(bool success,const QString &error) {
    if(pendingPath.isEmpty())return;
    success=success && QFileInfo(pendingPath).size()>0 && QFile::rename(pendingPath,finalPath);
    const QString result=success?finalPath:("备份失败："+error);
    if(!success)QFile::remove(pendingPath);
    pendingPath.clear();
    emit finished(success,result);
}
