// 【阅读导引】
// 应用数据库连接单例：首次访问时配置并打开连接；窗口和服务销毁后再移除连接。
// 该单例不是跨线程数据库连接池；不要把返回的句柄随意拿到采集/录像线程使用。

// 实现 MySQL 连接的懒加载、状态查询和安全释放流程。
#include "dbconn.h"

// 懒汉式：只初始化空指针，首次访问时才连接数据库。
DbConn* DbConn::dbConn = nullptr;

// 构造对象并初始化所需成员；冒号后的初始化列表先于函数体执行。
DbConn::DbConn()
    : db(QSqlDatabase::addDatabase(qEnvironmentVariable("SMART_MONITOR_DB_DRIVER", "QMYSQL")))
{
    db.setHostName(qEnvironmentVariable("SMART_MONITOR_DB_HOST", "127.0.0.1"));
    db.setPort(3306);
    db.setUserName(qEnvironmentVariable("SMART_MONITOR_DB_USER", "root"));
    db.setPassword(qEnvironmentVariable("SMART_MONITOR_DB_PASSWORD", "123456"));
    db.setDatabaseName(qEnvironmentVariable("SMART_MONITOR_DB_NAME", "smart_monitor_db"));

    if (!db.open()) {
        qDebug() << "数据库连接失败："
                 << db.lastError().text();
        return;
    }

    qDebug() << "数据库连接成功";
}

// 对象销毁时释放本类负责的资源；依赖的外部对象需按约定保持有效。
DbConn::~DbConn()
{
    qDebug() << "开始释放数据库连接";

    QString connectionName = db.connectionName();

    if (db.isOpen()) {
        db.close();
    }

    // removeDatabase 前必须解除成员对连接的引用，否则 Qt 会警告连接仍在使用。
    db = QSqlDatabase();

    // 从Qt数据库连接管理器中删除
    if (QSqlDatabase::contains(connectionName)) {
        QSqlDatabase::removeDatabase(connectionName);
    }

    qDebug() << "数据库连接已经释放";
}

// 首次访问创建连接单例，后续返回相同对象；不具备跨线程连接池语义。
DbConn* DbConn::getInstance()
{
    if (dbConn == nullptr) {
        dbConn = new DbConn;
    }
    return dbConn;
}

// 返回共享底层连接的 QSqlDatabase 句柄副本，不创建新连接。
QSqlDatabase DbConn::database() const
{
    return db;
}

// 返回 Qt 当前连接打开标志，不等同于对服务器进行实时健康探测。
bool DbConn::isOpen() const
{
    return db.isOpen();
}

// 返回连接最近的错误文字，不在日志输出连接密码。
QString DbConn::lastErrorText() const
{
    return db.lastError().text();
}

// 释放连接相关对象并清空单例，必须在依赖它的窗口/查询释放后执行。
void DbConn::releaseDbConn()
{
    if (dbConn != nullptr) {
        delete dbConn;
        dbConn = nullptr;
    }
}
