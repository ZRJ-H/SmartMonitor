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

bool DbConnLazy::isOpen() const
{
    return db.isOpen();
}

QString DbConnLazy::lastErrorText() const
{
    return db.lastError().text();
}

void DbConnLazy::releaseDbConn()
{
    if (instance != nullptr) {
        delete instance;
        instance = nullptr;
    }
}
