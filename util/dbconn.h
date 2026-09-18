#ifndef DBCONN_H
#define DBCONN_H

#include <QDebug>
#include <QSqlDatabase>
#include <QSqlError>
#include <QString>

class DbConn
{
public:
    // 首次调用时创建的懒汉式数据库单例
    static DbConn* getInstance();

    // 返回数据库连接句柄，Model 用它创建短生命周期的局部查询对象。
    QSqlDatabase database() const;

    // 登录窗口通过这两个接口区分连接失败和账号校验失败
    bool isOpen() const;
    QString lastErrorText() const;

    // 释放数据库单例
    static void releaseDbConn();

    // 禁止拷贝构造
    DbConn(const DbConn&) = delete;

    // 禁止拷贝赋值
    DbConn& operator=(const DbConn&) = delete;

private:
    DbConn();
    ~DbConn();

    // 数据库单例指针
    static DbConn* dbConn;

    QSqlDatabase db;
};

#endif // DBCONN_H
