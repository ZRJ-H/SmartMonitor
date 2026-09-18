#ifndef AUTHSESSION_H
#define AUTHSESSION_H

#include <QString>

enum class AuthPermission
{
    LivePreview,
    Playback,
    Settings
};

class AuthSession
{
public:
    AuthSession();

    bool isLoggedIn() const;
    QString userName() const;
    bool canAccess(AuthPermission permission) const;
    void login(const QString &userName);
    void logout();

private:
    QString currentUserName;
};

#endif // AUTHSESSION_H
