#include "backupservice.h"
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcessEnvironment>
#include <QTimer>
#include <QUuid>
BackupService::BackupService(QObject *parent):QObject(parent) {
    connect(&process,&QProcess::finished,this,[this](int code,QProcess::ExitStatus status){
        finish(code==0 && status==QProcess::NormalExit, QString::fromLocal8Bit(process.readAllStandardError()));
    });
    connect(&process,&QProcess::errorOccurred,this,[this](QProcess::ProcessError error){
        if(error==QProcess::FailedToStart) finish(false,process.errorString());
    });
}
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
void BackupService::finish(bool success,const QString &error) {
    if(pendingPath.isEmpty())return;
    success=success && QFileInfo(pendingPath).size()>0 && QFile::rename(pendingPath,finalPath);
    const QString result=success?finalPath:("备份失败："+error);
    if(!success)QFile::remove(pendingPath);
    pendingPath.clear();
    emit finished(success,result);
}
