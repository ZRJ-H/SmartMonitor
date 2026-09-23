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
    int userId() const;
    QString userName() const;
    bool canAccess(AuthPermission permission) const;
    void login(int userId, const QString &userName);
    void logout();

private:
    int currentUserId;
    QString currentUserName;
};

#endif // AUTHSESSION_H
