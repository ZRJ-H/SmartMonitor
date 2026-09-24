// 【阅读导引】
// 账户持久化：仅访问 admin_user；参数通过 bindValue 传入，结果或错误返回业务层。

// 实现用户凭据查询与最后登录时间更新，SQL 参数均通过绑定变量传入。
#include "usermodel.h"
#include "../util/dbconn.h"

#include <QSqlError>
#include <QSqlQuery>

// 构造对象并初始化所需成员；冒号后的初始化列表先于函数体执行。
UserModel::UserModel()
    : last_error_text()
{
}

// 仅匹配启用账户；查无记录返回 false 且无 SQL 错误，业务层据此计失败次数。
bool UserModel::queryUserAndPwd(QString &username,
                                QString &password,
                                int &userId)
{
    last_error_text.clear();
    userId = -1;

    DbConn *connection = DbConn::getInstance();
    if (connection == nullptr || !connection->isOpen()) {
        last_error_text = connection == nullptr
                ? QStringLiteral("数据库连接尚未创建")
                : connection->lastErrorText();
        return false;
    }

    QSqlQuery query(connection->database());
    query.prepare(
        "SELECT id, username "
        "FROM admin_user "
        "WHERE username = :username "
        "AND password = :password "
        "AND status = 1 "
        "LIMIT 1"
    );
    query.bindValue(":username", username);
    query.bindValue(":password", password);

    if (!query.exec()) {
        last_error_text = query.lastError().text();
        return false;
    }
    if (!query.next()) {
        return false;
    }

    userId = query.value("id").toInt();
    username = query.value("username").toString();
    return true;
}

// 更新指定账户的最近登录时间；返回受影响行数，SQL 失败返回 -1。
int UserModel::updateLoginTime(int &userId)
{
    last_error_text.clear();

    DbConn *connection = DbConn::getInstance();
    if (connection == nullptr || !connection->isOpen()) {
        last_error_text = connection == nullptr
                ? QStringLiteral("数据库连接尚未创建")
                : connection->lastErrorText();
        return -1;
    }

    QSqlQuery query(connection->database());
    query.prepare(
        "UPDATE admin_user "
        "SET last_login_time = CURRENT_TIMESTAMP "
        "WHERE id = :id"
    );
    query.bindValue(":id", userId);

    if (!query.exec()) {
        last_error_text = query.lastError().text();
        return -1;
    }
    return query.numRowsAffected();
}

// 获取最近一次数据库调用的错误详情，供业务区分故障和凭据不匹配。
QString UserModel::lastErrorText() const
{
    return last_error_text;
}

// 直接插入并依靠唯一键处理并发重名；失败后查询确认是否属于重复账号。
UserModel::CreateResult UserModel::createUser(const QString &username, const QString &passwordHash)
{
    last_error_text.clear();
    auto *connection = DbConn::getInstance();
    if (!connection->isOpen()) {
        last_error_text = connection->lastErrorText();
        return CreateError;
    }
    QSqlQuery query(connection->database());
    query.prepare("INSERT INTO admin_user (username, password, status) VALUES (:name, :password, 1)");
    query.bindValue(":name", username);
    query.bindValue(":password", passwordHash);
    if (query.exec()) return Created;
    last_error_text = query.lastError().text();
    // Let the unique database key arbitrate concurrent registrations too.
    QSqlQuery existing(connection->database());
    existing.prepare("SELECT id FROM admin_user WHERE username = :name");
    existing.bindValue(":name", username);
    if (existing.exec() && existing.next()) {
        last_error_text.clear();
        return Duplicate;
    }
    return CreateError;
}
