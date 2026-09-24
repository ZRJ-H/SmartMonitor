// 【阅读导引】
// 登录/注册界面：收集输入、校验验证码，解释 UserService 返回的业务结果并发出窗口信号。

// 声明登录窗口，负责采集账号、密码和验证码输入，并将登录结果通过信号交给窗口管理器。
#ifndef LOGINWIDGET_H
#define LOGINWIDGET_H

#include "../service/userservice.h"

#include <QString>
#include <QWidget>

class QCloseEvent;
class QLineEdit;
class VerifyCodeLabel;

// 登录窗口只负责收集和校验输入，通过信号把结果通知主窗口。
class LoginWidget : public QWidget
{
    // login_success/login_cancelled 是自定义信号，必须启用元对象系统。
    Q_OBJECT

public:
    // 构建登录表单、验证码控件并连接按钮及回车信号。
    explicit LoginWidget(QWidget *parent = nullptr);

signals:
    // 自定义信号只声明，不手写定义；moc 会生成信号实现。
    // 信号表示“事情已经发生”，而不直接操作 MainWidget，从而降低耦合。
    void login_success(int userId, const QString &userName); // 验证成功，携带已登录用户名。
    void login_cancelled(); // 用户取消或关闭登录窗口，不改变主窗口会话。

public slots:
    // 登录按钮和输入框回车都复用 login()，避免重复编写校验代码。
    void login();
    // 发布取消信号，由 WindowManager 决定后续窗口切换。
    void cancel();

protected:
    // 点窗口右上角关闭按钮时，也要通知主界面恢复显示。
    void closeEvent(QCloseEvent *event) override;

private:
    // 这些控件需要在 login() 中继续访问，因此保存为成员变量。
    QLineEdit *edit_user;       // 管理员账号输入框。
    QLineEdit *edit_pwd;        // 密码输入框，界面以密码模式显示。
    QLineEdit *edit_vcode;      // 用户输入的图形验证码。
    VerifyCodeLabel *pic_vcode; // 当前验证码图片，可点击刷新。

    // View 只调用 Service，不直接访问数据库。
    UserService user_service; // 执行格式校验、失败限流和账号数据库验证。
};

#endif // LOGINWIDGET_H
