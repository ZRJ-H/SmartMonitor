#include "userservice.h"

#include <QCryptographicHash>

UserService::UserService()
    : user_model()
{
}

LoginResult UserService::login(const QString &userName,
                               const QString &plainPassword)
{
    LoginResult result;
    QString confirmed_name = userName;
    QString password_md5 = md5Text(plainPassword);
    int user_id = -1;

    if (!user_model.queryUserAndPwd(confirmed_name, password_md5, user_id)) {
        if (!user_model.lastErrorText().isEmpty()) {
            result.status = LoginResult::DatabaseError;
            result.errorText = user_model.lastErrorText();
        }
        return result;
    }

    if (user_model.updateLoginTime(user_id) < 0) {
        result.status = LoginResult::DatabaseError;
        result.errorText = user_model.lastErrorText().isEmpty()
                ? QStringLiteral("更新最近登录时间失败")
                : user_model.lastErrorText();
        return result;
    }

    result.status = LoginResult::Success;
    result.userId = user_id;
    result.userName = confirmed_name;
    return result;
}

QString UserService::md5Text(const QString &text)
{
    const QByteArray digest = QCryptographicHash::hash(
        text.toUtf8(),
        QCryptographicHash::Md5
    );
    return QString::fromLatin1(digest.toHex());
}
