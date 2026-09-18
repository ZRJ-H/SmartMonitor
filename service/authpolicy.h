#ifndef AUTHPOLICY_H
#define AUTHPOLICY_H

#include <QDateTime>
#include <QHash>
#include <QString>

class AuthPolicy
{
public:
    static bool isUserNameValid(const QString &userName);
    static bool isPasswordValid(const QString &password);
};

struct LoginFailure
{
    LoginFailure()
        : locked(false), remainingAttempts(3), lockSeconds(0)
    {
    }

    bool locked;
    int remainingAttempts;
    int lockSeconds;
};

class LoginThrottle
{
public:
    LoginThrottle();

    bool isLocked(const QString &userName,
                  const QDateTime &now,
                  int *remainingSeconds = nullptr);
    LoginFailure recordFailure(const QString &userName,
                               const QDateTime &now);
    void recordSuccess(const QString &userName);

private:
    struct AttemptState
    {
        AttemptState() : failures(0) {}

        int failures;
        QDateTime lockedUntil;
    };

    static const int MaxFailures = 3;
    static const int LockDurationSeconds = 600;
    QHash<QString, AttemptState> attempts;
};

#endif // AUTHPOLICY_H
