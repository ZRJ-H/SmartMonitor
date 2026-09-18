#include "dbconn.h"

// 懒汉式：只初始化空指针，首次访问时才连接数据库。
DbConn* DbConn::dbConn = nullptr;

DbConn::DbConn()
    : db(QSqlDatabase::addDatabase("QMYSQL"))
{
    db.setHostName("127.0.0.1");
    db.setPort(3306);
    db.setUserName("root");
    db.setPassword("123456");
    db.setDatabaseName("smart_monitor_db");

    if (!db.open()) {
        qDebug() << "数据库连接失败："
                 << db.lastError().text();
        return;
    }

    qDebug() << "数据库连接成功";
}

DbConn::~DbConn()
{
    qDebug() << "开始释放数据库连接";

    QString connectionName = db.connectionName();

    if (db.isOpen()) {
        db.close();
    }

    // 解除db成员对连接的引用
    db = QSqlDatabase();

    // 从Qt数据库连接管理器中删除
    if (QSqlDatabase::contains(connectionName)) {
        QSqlDatabase::removeDatabase(connectionName);
    }

    qDebug() << "数据库连接已经释放";
}

DbConn* DbConn::getInstance()
{
    if (dbConn == nullptr) {
        dbConn = new DbConn;
    }
    return dbConn;
}

QSqlDatabase DbConn::database() const
{
    return db;
}

bool DbConn::isOpen() const
{
    return db.isOpen();
}

QString DbConn::lastErrorText() const
{
    return db.lastError().text();
}

void DbConn::releaseDbConn()
{
    if (dbConn != nullptr) {
        delete dbConn;
        dbConn = nullptr;
    }
}
