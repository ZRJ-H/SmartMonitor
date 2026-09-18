#ifndef USERSERVICE_H
#define USERSERVICE_H

#include "../model/usermodel.h"

#include <QString>

struct LoginResult
{
    enum Status { Success, InvalidCredentials, DatabaseError };

    LoginResult() : status(InvalidCredentials), userId(-1) {}

    Status status;
    int userId;
    QString userName;
    QString errorText;
};

class UserService
{
public:
    UserService();
    LoginResult login(const QString &userName, const QString &plainPassword);

private:
    static QString md5Text(const QString &text);
    UserModel user_model;
};

#endif // USERSERVICE_H
