// 【阅读导引】
// 应用数据库连接单例：首次访问时配置并打开连接；窗口和服务销毁后再移除连接。
// 该单例不是跨线程数据库连接池；不要把返回的句柄随意拿到采集/录像线程使用。

// 声明应用共享的 MySQL 连接单例，为各 Model 提供连接状态、错误信息和数据库句柄。
#ifndef DBCONN_H
#define DBCONN_H

#include <QDebug>
#include <QSqlDatabase>
#include <QSqlError>
#include <QString>

// 懒加载数据库连接管理器：首次访问时连接 smart_monitor_db，退出时显式释放。
class DbConn
{
public:
    // 首次调用时创建的懒汉式数据库单例
    static DbConn* getInstance();

    // 返回数据库连接句柄，Model 用它创建短生命周期的局部查询对象。
    QSqlDatabase database() const;

    // Model/Service 通过这两个接口识别并向上层传递数据库连接失败。
    bool isOpen() const;
    QString lastErrorText() const;

    // 释放数据库单例
    static void releaseDbConn();

    // 禁止拷贝构造
    DbConn(const DbConn&) = delete;

    // 禁止拷贝赋值
    DbConn& operator=(const DbConn&) = delete;

private:
    // 构造时配置并打开连接；失败原因保留在 QSqlDatabase 中。
    DbConn();
    // 关闭连接并从 Qt 全局连接池中移除连接名。
    ~DbConn();

    // 数据库单例指针
    static DbConn* dbConn;

    QSqlDatabase db; // Qt 数据库连接句柄，Model 通过副本共享同一底层连接。
};

#endif // DBCONN_H
