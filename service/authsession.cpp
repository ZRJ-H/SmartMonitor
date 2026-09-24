// 【阅读导引】
// 会话状态：保存已认证用户的 ID/名称；当前所有权限类别统一要求登录，不划分管理员角色。

#include "authsession.h"

// 构造对象并初始化所需成员；冒号后的初始化列表先于函数体执行。
AuthSession::AuthSession()
    : currentUserId(0),
      currentUserName()
{
}

// 以当前用户名是否为空判断已登录状态，身份只能经 login/logout 更新。
bool AuthSession::isLoggedIn() const
{
    return !currentUserName.isEmpty();
}

// 返回认证保存的数据库用户 ID，用于审计关联。
int AuthSession::userId() const
{
    return currentUserId;
}

// 返回当前显示用户名，不能用它代替稳定的数据库 ID。
QString AuthSession::userName() const
{
    return currentUserName;
}

// 当前不区分权限枚举类别：所有受控操作都要求有效登录。
bool AuthSession::canAccess(AuthPermission permission) const
{
    Q_UNUSED(permission);
    return isLoggedIn();
}

// 拒绝非法 ID/空名称，否则保存认证结果；不在这里重新查询密码。
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

// 清零身份并清空名称；窗口隐藏和播放器停止由界面协调。
void AuthSession::logout()
{
    currentUserId = 0;
    currentUserName.clear();
}
