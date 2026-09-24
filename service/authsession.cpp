#include "authsession.h"

AuthSession::AuthSession()
    : currentUserId(0),
      currentUserName()
{
}

bool AuthSession::isLoggedIn() const
{
    return !currentUserName.isEmpty();
}

int AuthSession::userId() const
{
    return currentUserId;
}

QString AuthSession::userName() const
{
    return currentUserName;
}

bool AuthSession::canAccess(AuthPermission permission) const
{
    Q_UNUSED(permission);
    return isLoggedIn();
}

void AuthSession::login(int userId, const QString &userName)
{
    if (userId < 1 || userName.trimmed().isEmpty()) {
        logout();
        return;
    }

    // 审计日志通过 admin_user.id 建立外键，必须保存认证结果中的真实编号，
    // 不能在记录日志时再按可能被修改的显示名称查询一次。
    currentUserId = userId;
    currentUserName = userName;
}

void AuthSession::logout()
{
    currentUserId = 0;
    currentUserName.clear();
}
