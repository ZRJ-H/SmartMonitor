// 【阅读导引】
// 登录策略：只判断输入格式与内存中的失败次数，不显示窗口、不执行 SQL。
// LoginFailure 是一次失败处理的结果快照；LoginThrottle 才拥有累计失败状态。

// 实现登录格式校验与基于当前时间的失败计数、到期解锁逻辑。
#include "authpolicy.h"

#include <QRegularExpression>

// 用整串正则匹配账号格式，不访问数据库判断是否已注册。
bool AuthPolicy::isUserNameValid(const QString &userName)
{
    static const QRegularExpression pattern(
        QStringLiteral("^[A-Za-z0-9_]{6,10}$")
    );
    return pattern.match(userName).hasMatch();
}

// 检查长度和空白字符；这里只是课程输入规则，不代表密码强度完整评估。
bool AuthPolicy::isPasswordValid(const QString &password)
{
    static const QRegularExpression pattern(QStringLiteral("^\\S{6,8}$"));
    return pattern.match(password).hasMatch();
}

// 构造对象并初始化所需成员；冒号后的初始化列表先于函数体执行。
LoginThrottle::LoginThrottle()
    : attempts()
{
}

// 检查给定时间是否早于解锁时间，到期删除状态；可选指针返回剩余秒数。
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

// 累加该用户名的失败次数，达到阈值设置截止时间，并返回本次结果快照。
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

// 认证成功后移除该用户的失败累计；其他用户不受影响。
void LoginThrottle::recordSuccess(const QString &userName)
{
    attempts.remove(userName);
}
