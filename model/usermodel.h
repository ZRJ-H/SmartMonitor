#ifndef USERMODEL_H
#define USERMODEL_H

#include <QString>

// Model 层：只负责 admin_user 表的数据访问。
class UserModel
{
public:
    UserModel();
    bool queryUserAndPwd(QString &username, QString &password, int &userId);
    int updateLoginTime(int &userId);
    QString lastErrorText() const;

private:
    QString last_error_text;
};

#endif // USERMODEL_H
