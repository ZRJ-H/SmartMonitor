#ifndef USERSERVICE_H
#define USERSERVICE_H

#include "../model/usermodel.h"
#include "authpolicy.h"

#include <QString>

struct LoginResult
{
    enum Status {
        Success,
        InvalidUserNameFormat,
        InvalidPasswordFormat,
        InvalidCredentials,
        Locked,
        DatabaseError
    };

    LoginResult()
        : status(InvalidCredentials),
          userId(-1),
          remainingAttempts(3),
          lockSeconds(0)
    {
    }

    Status status;
    int userId;
    QString userName;
    QString errorText;
    int remainingAttempts;
    int lockSeconds;
};

class UserService
{
public:
    UserService();
    LoginResult login(const QString &userName, const QString &plainPassword);

private:
    static QString md5Text(const QString &text);
    UserModel user_model;
    LoginThrottle login_throttle;
};

#endif // USERSERVICE_H
