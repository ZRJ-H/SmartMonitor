// 实现登录用例的完整决策顺序：格式校验、锁定检查、凭据查询、失败计数和成功收尾。
#include "userservice.h"

#include <QCryptographicHash>
#include <QDateTime>

UserService::UserService()
    : user_model(),
      login_throttle()
{
}

LoginResult UserService::login(const QString &userName,
                               const QString &plainPassword)
{
    LoginResult result;
    QString confirmed_name = userName.trimmed();

    if (!AuthPolicy::isUserNameValid(confirmed_name)) {
        result.status = LoginResult::InvalidUserNameFormat;
        return result;
    }
    if (!AuthPolicy::isPasswordValid(plainPassword)) {
        result.status = LoginResult::InvalidPasswordFormat;
        return result;
    }

    const QDateTime now = QDateTime::currentDateTime();
    if (login_throttle.isLocked(confirmed_name, now, &result.lockSeconds)) {
        result.status = LoginResult::Locked;
        result.remainingAttempts = 0;
        return result;
    }

    QString password_md5 = md5Text(plainPassword);
    int user_id = -1;

    if (!user_model.queryUserAndPwd(confirmed_name, password_md5, user_id)) {
        if (!user_model.lastErrorText().isEmpty()) {
            result.status = LoginResult::DatabaseError;
            result.errorText = user_model.lastErrorText();
            return result;
        }

        const LoginFailure failure =
            login_throttle.recordFailure(confirmed_name, now);
        result.remainingAttempts = failure.remainingAttempts;
        result.lockSeconds = failure.lockSeconds;
        if (failure.locked) {
            result.status = LoginResult::Locked;
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
    login_throttle.recordSuccess(confirmed_name);
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
