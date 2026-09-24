// 声明 admin_user 表的数据访问对象，封装账号查询和最后登录时间更新。
#ifndef USERMODEL_H
#define USERMODEL_H

#include <QString>

// Model 层：只负责 admin_user 表的数据访问。
class UserModel
{
public:
    UserModel();
    enum CreateResult { Created, Duplicate, CreateError };
    CreateResult createUser(const QString &username, const QString &passwordHash);
    // 按用户名和已加密密码查询用户；成功时写入 userId，匹配到账号返回 true。
    bool queryUserAndPwd(QString &username, QString &password, int &userId);
    // 更新指定用户的最后登录时间；返回受影响的记录数，失败返回负值。
    int updateLoginTime(int &userId);
    // 返回最近一次数据库操作失败时的错误文本。
    QString lastErrorText() const;

private:
    QString last_error_text; // 保存最近一次查询或更新失败的原因。
};

#endif // USERMODEL_H
