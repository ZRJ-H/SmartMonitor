// 实现登录格式校验与基于当前时间的失败计数、到期解锁逻辑。
#include "authpolicy.h"

#include <QRegularExpression>

bool AuthPolicy::isUserNameValid(const QString &userName)
{
    static const QRegularExpression pattern(
        QStringLiteral("^[A-Za-z0-9_]{6,10}$")
    );
    return pattern.match(userName).hasMatch();
}

bool AuthPolicy::isPasswordValid(const QString &password)
{
    static const QRegularExpression pattern(QStringLiteral("^\\S{6,8}$"));
    return pattern.match(password).hasMatch();
}

LoginThrottle::LoginThrottle()
    : attempts()
{
}

bool LoginThrottle::isLocked(const QString &userName,
                             const QDateTime &now,
                             int *remainingSeconds)
{
    if (remainingSeconds != nullptr) {
        *remainingSeconds = 0;
    }

    auto it = attempts.find(userName);
    if (it == attempts.end() || !it->lockedUntil.isValid()) {
        return false;
    }

    const qint64 seconds = now.secsTo(it->lockedUntil);
    if (seconds <= 0) {
        // 锁定到期即移除旧状态，使下一次失败从零重新累计。
        attempts.erase(it);
        return false;
    }

    if (remainingSeconds != nullptr) {
        *remainingSeconds = static_cast<int>(seconds);
    }
    return true;
}

LoginFailure LoginThrottle::recordFailure(const QString &userName,
                                          const QDateTime &now)
{
    LoginFailure result;
    int remainingSeconds = 0;
    if (isLocked(userName, now, &remainingSeconds)) {
        result.locked = true;
        result.remainingAttempts = 0;
        result.lockSeconds = remainingSeconds;
        return result;
    }

    AttemptState &state = attempts[userName];
    ++state.failures;
    result.remainingAttempts = MaxFailures - state.failures;

    if (state.failures >= MaxFailures) {
        state.lockedUntil = now.addSecs(LockDurationSeconds);
        result.locked = true;
        result.remainingAttempts = 0;
        result.lockSeconds = LockDurationSeconds;
    }
    return result;
}

void LoginThrottle::recordSuccess(const QString &userName)
{
    attempts.remove(userName);
}
