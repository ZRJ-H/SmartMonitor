#ifndef DBCONN_LAZY_H
#define DBCONN_LAZY_H

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QString>

// 懒汉式数据库连接单例演示。
// 与现有 DbConn 饿汉式不同：这里只把 instance 初始化为 nullptr，
// 第一次调用 getInstance() 时才真正执行 new DbConnLazy。
//
// 调用示例（本文件只是 Demo，现有工程暂未切换）：
//
// 1. 第一次获取对象，同时创建并打开数据库连接
//    DbConnLazy *dbConn = DbConnLazy::getInstance();
//
// 2. 判断连接是否成功
//    if (!dbConn->isOpen()) {
//        qDebug() << "数据库连接失败" << dbConn->lastErrorText();
//        return;
//    }
//
// 3. 复用单例内部的查询对象
//    QSqlQuery &query = dbConn->getQuery();
//    query.prepare("SELECT * FROM admin_user WHERE username = :username");
//    query.bindValue(":username", "anon");
//    if (query.exec()) {
//        while (query.next()) {
//            qDebug() << query.value("username").toString();
//        }
//    }
//
// 4. 程序退出前释放。释放后再次调用 getInstance() 会重新创建实例
//    DbConnLazy::releaseDbConn();
class DbConnLazy
{
public:
    // 第一次调用时创建对象，以后都返回同一个地址。
    static DbConnLazy *getInstance();

    // 返回单例内部维护的查询对象。
    QSqlQuery &getQuery();

    bool isOpen() const;
    QString lastErrorText() const;

    // 释放查询对象和数据库连接，并把 instance 恢复为 nullptr。
    static void releaseDbConn();

    // 单例不允许复制出第二个对象。
    DbConnLazy(const DbConnLazy &) = delete;
    DbConnLazy &operator=(const DbConnLazy &) = delete;

private:
    DbConnLazy();
    ~DbConnLazy();

    // 懒汉式核心：类外只初始化为 nullptr，不在程序启动时创建对象。
    static DbConnLazy *instance;

    // 声明顺序决定构造顺序：先创建 db，再用 db 构造 sqlQuery。
    QSqlDatabase db;
    QSqlQuery sqlQuery;
};

#endif // DBCONN_LAZY_H
