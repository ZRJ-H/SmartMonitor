// 声明用户登录业务：组合输入校验、失败限流、密码摘要和用户表访问。
#ifndef USERSERVICE_H
#define USERSERVICE_H

#include "../model/usermodel.h"
#include "authpolicy.h"

#include <QString>

// 登录业务结果，既包含状态枚举，也携带界面展示所需的用户、错误和限流信息。
struct LoginResult
{
    // 覆盖登录流程中所有可区分的业务结果。
    enum Status {
        Success,               // 凭据正确且登录时间更新成功。
        InvalidUserNameFormat, // 用户名未通过格式校验。
        InvalidPasswordFormat, // 密码未通过格式校验。
        InvalidCredentials,    // 数据库可用，但账号或密码不匹配。
        Locked,                // 账号因连续失败处于临时锁定状态。
        DatabaseError          // 查询或更新用户表失败。
    };

    LoginResult()
        : status(InvalidCredentials),
          userId(-1),
          remainingAttempts(3),
          lockSeconds(0)
    {
    }

    Status status;         // 本次登录的最终状态。
    int userId;            // 成功时的数据库用户编号，否则为 -1。
    QString userName;      // 成功时返回规范化后的用户名。
    QString errorText;     // 数据库异常等需要向上层展示的详细原因。
    int remainingAttempts; // 凭据错误后剩余的尝试次数。
    int lockSeconds;       // 锁定状态的剩余或新产生秒数。
};

// 登录用例服务，界面只需提交明文输入并解释 LoginResult。
class UserService
{
public:
    UserService();
    // 验证输入、检查限流、查询用户并更新时间；返回完整业务结果。
    LoginResult login(const QString &userName, const QString &plainPassword);

private:
    // 将明文转换为数据库使用的 MD5 十六进制字符串。
    static QString md5Text(const QString &text);
    UserModel user_model;          // admin_user 表的数据访问对象。
    LoginThrottle login_throttle;  // 会话期内按用户名记录连续失败次数。
};

#endif // USERSERVICE_H
