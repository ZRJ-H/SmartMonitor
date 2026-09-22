// 定义登录输入格式校验和失败次数限流策略，不负责访问数据库或显示界面。
#ifndef AUTHPOLICY_H
#define AUTHPOLICY_H

#include <QDateTime>
#include <QHash>
#include <QString>

// 无状态的账号、密码格式校验器。
class AuthPolicy
{
public:
    // 用户名必须为 6～10 位字母、数字或下划线。
    static bool isUserNameValid(const QString &userName);
    // 密码必须为 6～8 位非空白字符。
    static bool isPasswordValid(const QString &password);
};

// 一次登录失败后的限流结果，供业务层生成明确的用户提示。
struct LoginFailure
{
    LoginFailure()
        : locked(false), remainingAttempts(3), lockSeconds(0)
    {
    }

    bool locked;          // 当前账号是否已被临时锁定。
    int remainingAttempts; // 触发锁定前还允许失败的次数。
    int lockSeconds;       // 锁定状态剩余或新产生的秒数。
};

// 按用户名记录连续失败次数；达到阈值后在内存中临时锁定账号。
class LoginThrottle
{
public:
    LoginThrottle();

    // 检查指定时刻账号是否仍被锁定；可选输出剩余秒数。
    bool isLocked(const QString &userName,
                  const QDateTime &now,
                  int *remainingSeconds = nullptr);
    // 记录一次失败并返回剩余尝试次数或新产生的锁定信息。
    LoginFailure recordFailure(const QString &userName,
                               const QDateTime &now);
    // 登录成功后清除该账号累计的失败状态。
    void recordSuccess(const QString &userName);

private:
    struct AttemptState
    {
        AttemptState() : failures(0) {}

        int failures;          // 当前连续登录失败次数。
        QDateTime lockedUntil; // 锁定截止时间；无效值表示尚未锁定。
    };

    static const int MaxFailures = 3;          // 连续失败三次触发锁定。
    static const int LockDurationSeconds = 600; // 每次锁定十分钟。
    QHash<QString, AttemptState> attempts;      // 以用户名为键保存会话期内的失败状态。
};

#endif // AUTHPOLICY_H
