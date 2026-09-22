// 保存主窗口当前登录身份，并集中判断实时预览、回放和设置页面的访问权限。
#ifndef AUTHSESSION_H
#define AUTHSESSION_H

#include <QString>

// 主窗口中需要鉴权的功能类别。
enum class AuthPermission
{
    LivePreview, // 实时预览允许未登录用户使用。
    Playback,    // 录像回放要求已登录。
    Settings     // 通道设置要求已登录。
};

// 轻量级会话对象：以非空用户名表示登录状态，不承担账号验证。
class AuthSession
{
public:
    AuthSession();

    bool isLoggedIn() const; // 返回当前是否保存了已登录用户名。
    QString userName() const; // 返回当前用户名；未登录时为空字符串。
    bool canAccess(AuthPermission permission) const; // 判断当前身份是否具有指定权限。
    void login(const QString &userName); // 写入验证成功的用户名，建立会话。
    void logout(); // 清空用户名，结束会话。

private:
    QString currentUserName; // 当前已登录用户名，也是登录状态的唯一来源。
};

#endif // AUTHSESSION_H
