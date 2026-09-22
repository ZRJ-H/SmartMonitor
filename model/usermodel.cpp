// 实现用户凭据查询与最后登录时间更新，SQL 参数均通过绑定变量传入。
#include "usermodel.h"
#include "../util/dbconn.h"

#include <QSqlError>
#include <QSqlQuery>

UserModel::UserModel()
    : last_error_text()
{
}

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
        "SET last_login_time = NOW() "
        "WHERE id = :id"
    );
    query.bindValue(":id", userId);

    if (!query.exec()) {
        last_error_text = query.lastError().text();
        return -1;
    }
    return query.numRowsAffected();
}

QString UserModel::lastErrorText() const
{
    return last_error_text;
}
