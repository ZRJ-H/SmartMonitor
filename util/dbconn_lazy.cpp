// 【阅读导引】
// 另一种懒汉数据库单例教学示例：长期持有 QSqlQuery；当前主工程未切换到此实现。

// 实现教学用 DbConnLazy 单例，演示 QSqlQuery 与数据库连接的构造、复用和销毁顺序。
#include "dbconn_lazy.h"

#include <QDebug>
#include <QSqlError>

//这里只创建一个空指针，不会调用构造函数，也不会连接数据库。
DbConnLazy *DbConnLazy::instance = nullptr;

DbConnLazy *DbConnLazy::getInstance()
{
    // 懒汉式核心：直到第一次访问时才创建对象。
    if (instance == nullptr) {
        instance = new DbConnLazy;
    }

    return instance;
}

// 构造对象并初始化所需成员；冒号后的初始化列表先于函数体执行。
DbConnLazy::DbConnLazy()
    // 使用独立连接名，避免这个 Demo 与现有 DbConn 的默认连接重名。
    : db(QSqlDatabase::addDatabase("QMYSQL", "dbconn_lazy_connection")),
      sqlQuery(db)
{
    db.setHostName("127.0.0.1");
    db.setPort(3306);
    db.setUserName("root");
    db.setPassword("123456");
    db.setDatabaseName("smart_monitor_db");

    if (!db.open()) {
        qDebug() << "懒汉式数据库连接失败："
                 << db.lastError().text();
        return;
    }

    qDebug() << "懒汉式数据库连接成功";
}

// 对象销毁时释放本类负责的资源；依赖的外部对象需按约定保持有效。
DbConnLazy::~DbConnLazy()
{
    // removeDatabase() 前，必须先解除所有 QSqlQuery/QSqlDatabase 引用。
    sqlQuery.finish();
    sqlQuery.clear();
    sqlQuery = QSqlQuery();

    const QString connectionName = db.connectionName();

    if (db.isOpen()) {
        db.close();
    }

    db = QSqlDatabase();

    if (QSqlDatabase::contains(connectionName)) {
        QSqlDatabase::removeDatabase(connectionName);
    }

    qDebug() << "懒汉式数据库连接已经释放";
}

QSqlQuery &DbConnLazy::getQuery()
{
    return sqlQuery;
}

// 返回 Qt 当前连接打开标志，不等同于对服务器进行实时健康探测。
bool DbConnLazy::isOpen() const
{
    return db.isOpen();
}

// 返回连接最近的错误文字，不在日志输出连接密码。
QString DbConnLazy::lastErrorText() const
{
    return db.lastError().text();
}

// 释放连接相关对象并清空单例，必须在依赖它的窗口/查询释放后执行。
void DbConnLazy::releaseDbConn()
{
    if (instance != nullptr) {
        delete instance;
        instance = nullptr;
    }
}
