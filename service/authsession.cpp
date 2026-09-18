#include "authsession.h"

AuthSession::AuthSession()
    : currentUserName()
{
}

bool AuthSession::isLoggedIn() const
{
    return !currentUserName.isEmpty();
}

QString AuthSession::userName() const
{
    return currentUserName;
}

bool AuthSession::canAccess(AuthPermission permission) const
{
    return permission == AuthPermission::LivePreview || isLoggedIn();
}

void AuthSession::login(const QString &userName)
{
    currentUserName = userName;
}

void AuthSession::logout()
{
    currentUserName.clear();
}
